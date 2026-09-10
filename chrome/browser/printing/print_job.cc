// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_job_worker.h"
#include "chrome/browser/printing/printer_query.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printed_document.h"



namespace printing {

namespace {

// Helper function to ensure `job` is valid until at least `callback` returns.
void HoldRefCallback(scoped_refptr<PrintJob> job, base::OnceClosure callback) {
  std::move(callback).Run();
}


}  // namespace


PrintJob::PrintJob(PrintJobManager* print_job_manager)
    : print_job_manager_(print_job_manager) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(print_job_manager_);
}

PrintJob::PrintJob() = default;

PrintJob::~PrintJob() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // The job should be finished (or at least canceled) when it is destroyed.
  DCHECK(!is_job_pending_);
  DCHECK(!is_canceling_);
  DCHECK(!worker_ || !worker_->IsRunning());

  for (auto& observer : observers_) {
    observer.OnDestruction();
  }
}

void PrintJob::Initialize(std::unique_ptr<PrinterQuery> query,
                          const std::u16string& name,
                          uint32_t page_count) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!worker_);
  DCHECK(!is_job_pending_);
  DCHECK(!is_canceling_);
  DCHECK(!document_);
  worker_ = query->TransferContextToNewWorker(this);
  worker_->Start();
  rfh_id_ = query->rfh_id();
  std::unique_ptr<PrintSettings> settings = query->ExtractSettings();


  auto new_doc = base::MakeRefCounted<PrintedDocument>(std::move(settings),
                                                       name, query->cookie());
  new_doc->set_page_count(page_count);
  UpdatePrintedDocument(new_doc);
}


void PrintJob::StartPrinting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!worker_->IsRunning() || is_job_pending_) {
    NOTREACHED();
  }

  constexpr bool capture_printing_time = true;
  if (capture_printing_time) {
    printing_start_time_ = base::TimeTicks::Now();
  }

  // Real work is done in `PrintJobWorker::StartPrinting()`.
  worker_->PostTask(
      FROM_HERE, base::BindOnce(&HoldRefCallback, base::WrapRefCounted(this),
                                base::BindOnce(&PrintJobWorker::StartPrinting,
                                               base::Unretained(worker_.get()),
                                               base::RetainedRef(document_))));
  // Set the flag right now.
  is_job_pending_ = true;

  print_job_manager_->OnStarted(this);
}

void PrintJob::Stop() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (quit_closure_) {
    // In case we're running a nested run loop to wait for a job to finish,
    // and we finished before the timeout, quit the nested loop right away.
    std::move(quit_closure_).Run();
  }

  // Be sure to live long enough.
  scoped_refptr<PrintJob> handle(this);

  if (worker_->IsRunning()) {
    ControlledWorkerShutdown();
  } else {
    // Flush the cached document.
    is_job_pending_ = false;
    ClearPrintedDocument();
  }
}

void PrintJob::Cancel() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_canceling_)
    return;

  is_canceling_ = true;

  for (auto& observer : observers_) {
    observer.OnCanceling();
  }

  if (worker_ && worker_->IsRunning()) {
    // Call this right now so it renders the context invalid. Do not use
    // InvokeLater since it would take too much time.
    worker_->Cancel();
  }
  OnFailed();
  is_canceling_ = false;
}

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
void PrintJob::CleanupAfterContentAnalysisDenial() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  worker_->CleanupAfterContentAnalysisDenial();
}
#endif

bool PrintJob::FlushJob(base::TimeDelta timeout) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Make sure the object outlive this message loop.
  scoped_refptr<PrintJob> handle(this);

  base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
  quit_closure_ = loop.QuitClosure();
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), timeout);

  loop.Run();

  return true;
}

bool PrintJob::is_job_pending() const {
  return is_job_pending_;
}

PrintedDocument* PrintJob::document() const {
  return document_.get();
}

const PrintSettings& PrintJob::settings() const {
  return document()->settings();
}

#if BUILDFLAG(IS_CHROMEOS)
void PrintJob::SetSource(PrintJob::Source source,
                         const std::string& source_id) {
  source_ = source;
  source_id_ = source_id;
}

PrintJob::Source PrintJob::source() const {
  return source_;
}

const std::string& PrintJob::source_id() const {
  return source_id_;
}
#endif  // BUILDFLAG(IS_CHROMEOS)


void PrintJob::UpdatePrintedDocument(
    scoped_refptr<PrintedDocument> new_document) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(new_document);

  document_ = new_document;
  if (worker_)
    SyncPrintedDocumentToWorker();
}

void PrintJob::ClearPrintedDocument() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!document_)
    return;

  document_ = nullptr;
  if (worker_)
    SyncPrintedDocumentToWorker();
}

void PrintJob::SyncPrintedDocumentToWorker() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(worker_);
  DCHECK(!is_job_pending_);
  worker_->PostTask(
      FROM_HERE,
      base::BindOnce(&HoldRefCallback, base::WrapRefCounted(this),
                     base::BindOnce(&PrintJobWorker::OnDocumentChanged,
                                    base::Unretained(worker_.get()),
                                    base::RetainedRef(document_))));
}


void PrintJob::OnFailed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Stop();

  print_job_manager_->OnFailed(this);

  for (auto& observer : observers_) {
    observer.OnFailed();
  }
}

void PrintJob::OnDocDone(int job_id, PrintedDocument* document) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (printing_start_time_.has_value()) {
    base::UmaHistogramMediumTimes(
        "Printing.PrintDuration.LocalPrinter.Success",
        base::TimeTicks::Now() - printing_start_time_.value());
    printing_start_time_.reset();
  }

  print_job_manager_->OnDocDone(this, document, job_id);

  for (auto& observer : observers_) {
    observer.OnDocDone(job_id, document);
  }

  // This will call `Stop()` and broadcast a `JOB_DONE` message.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PrintJob::OnDocumentDone, this));
}

void PrintJob::OnDocumentDone() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Be sure to live long enough. The instance could be destroyed by the
  // `JOB_DONE` broadcast.
  scoped_refptr<PrintJob> handle(this);

  // Stop the worker thread.
  Stop();

  print_job_manager_->OnJobDone(this);

  for (auto& observer : observers_) {
    observer.OnJobDone();
  }
}

void PrintJob::ControlledWorkerShutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The deadlock this code works around is specific to window messaging on
  // Windows, so we aren't likely to need it on any other platforms.

  // Now make sure the thread object is cleaned up. Do this on a worker
  // thread because it may block.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&PrintJobWorker::Stop, base::Unretained(worker_.get())),
      base::BindOnce(&PrintJob::HoldUntilStopIsCalled, this));

  is_job_pending_ = false;
  ClearPrintedDocument();
}

bool PrintJob::PostTask(const base::Location& from_here,
                        base::OnceClosure task) {
  return content::GetUIThreadTaskRunner({})->PostTask(from_here,
                                                      std::move(task));
}

void PrintJob::HoldUntilStopIsCalled() {
}

void PrintJob::set_job_pending_for_testing(bool pending) {
  is_job_pending_ = pending;
}

void PrintJob::AddObserver(Observer& observer) {
  observers_.AddObserver(&observer);
}

void PrintJob::RemoveObserver(Observer& observer) {
  observers_.RemoveObserver(&observer);
}

}  // namespace printing

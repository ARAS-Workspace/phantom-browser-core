# History Component

Component that manages visited URLs history.

## History services

There are 2 different history services that fulfill different requirements:

* [HistoryService](/components/history/core/browser/history_service.h): manages
and allows access to local-only history.
* [BrowsingHistoryService](/components/history/core/browser/browsing_history_service.h):
is a layer on top of the service above.

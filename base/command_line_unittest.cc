// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/debug/debugging_buildflags.h"
#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_COMMANDLINE_SEQUENCE_CHECKS)
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#endif  // BUILDFLAG(ENABLE_COMMANDLINE_SEQUENCE_CHECKS)

namespace base {

// It should be parsed by Windows as: q"bs1\bs2\\bs3q\"
// Here that is with C-style escapes.
static const CommandLine::StringType kTricky =
    FILE_PATH_LITERAL("q\"bs1\\bs2\\\\bs3q\\\"");

TEST(CommandLineTest, CommandLineConstructor) {
  const CommandLine::CharType* argv[] = {
      FILE_PATH_LITERAL("program"),
      FILE_PATH_LITERAL("--foo="),
      FILE_PATH_LITERAL("-bAr"),
      FILE_PATH_LITERAL("-spaetzel=pierogi"),
      FILE_PATH_LITERAL("-baz"),
      FILE_PATH_LITERAL("flim"),
      FILE_PATH_LITERAL("--other-switches=--dog=canine --cat=feline"),
      FILE_PATH_LITERAL("-spaetzle=Crepe"),
      FILE_PATH_LITERAL("-=loosevalue"),
      FILE_PATH_LITERAL("-"),
      FILE_PATH_LITERAL("FLAN"),
      FILE_PATH_LITERAL("a"),
      FILE_PATH_LITERAL("--input-translation=45--output-rotation"),
      FILE_PATH_LITERAL("--"),
      FILE_PATH_LITERAL("--"),
      FILE_PATH_LITERAL("--not-a-switch"),
      FILE_PATH_LITERAL("\"in the time of submarines...\""),
      FILE_PATH_LITERAL("unquoted arg-with-space")};
  CommandLine cl(std::size(argv), argv);

  EXPECT_FALSE(cl.GetCommandLineString().empty());
  EXPECT_FALSE(cl.HasSwitch("cruller"));
  EXPECT_FALSE(cl.HasSwitch("flim"));
  EXPECT_FALSE(cl.HasSwitch("program"));
  EXPECT_FALSE(cl.HasSwitch("dog"));
  EXPECT_FALSE(cl.HasSwitch("cat"));
  EXPECT_FALSE(cl.HasSwitch("output-rotation"));
  EXPECT_FALSE(cl.HasSwitch("not-a-switch"));
  EXPECT_FALSE(cl.HasSwitch("--"));

  EXPECT_EQ(FilePath(FILE_PATH_LITERAL("program")).value(),
            cl.GetProgram().value());

  EXPECT_TRUE(cl.HasSwitch("foo"));
  EXPECT_FALSE(cl.HasSwitch("bar"));
  EXPECT_TRUE(cl.HasSwitch("baz"));
  EXPECT_TRUE(cl.HasSwitch("spaetzle"));
  EXPECT_TRUE(cl.HasSwitch("other-switches"));
  EXPECT_TRUE(cl.HasSwitch("input-translation"));

  EXPECT_EQ("Crepe", cl.GetSwitchValueASCII("spaetzle"));
  EXPECT_EQ("", cl.GetSwitchValueASCII("foo"));
  EXPECT_EQ("", cl.GetSwitchValueASCII("bar"));
  EXPECT_EQ("", cl.GetSwitchValueASCII("cruller"));
  EXPECT_EQ("--dog=canine --cat=feline",
            cl.GetSwitchValueASCII("other-switches"));
  EXPECT_EQ("45--output-rotation", cl.GetSwitchValueASCII("input-translation"));

  const CommandLine::StringVector& args = cl.GetArgs();
  ASSERT_EQ(8U, args.size());

  auto iter = args.begin();
  EXPECT_EQ(FILE_PATH_LITERAL("flim"), *iter);
  ++iter;
  EXPECT_EQ(FILE_PATH_LITERAL("-"), *iter);
  ++iter;
  EXPECT_EQ(FILE_PATH_LITERAL("FLAN"), *iter);
  ++iter;
  EXPECT_EQ(FILE_PATH_LITERAL("a"), *iter);
  ++iter;
  EXPECT_EQ(FILE_PATH_LITERAL("--"), *iter);
  ++iter;
  EXPECT_EQ(FILE_PATH_LITERAL("--not-a-switch"), *iter);
  ++iter;
  EXPECT_EQ(FILE_PATH_LITERAL("\"in the time of submarines...\""), *iter);
  ++iter;
  EXPECT_EQ(FILE_PATH_LITERAL("unquoted arg-with-space"), *iter);
  ++iter;
  EXPECT_TRUE(iter == args.end());
}

TEST(CommandLineTest, CommandLineFromArgvWithoutProgram) {
  CommandLine::StringVector argv = {FILE_PATH_LITERAL("--switch1"),
                                    FILE_PATH_LITERAL("--switch2=value2")};

  CommandLine cl = CommandLine::FromArgvWithoutProgram(argv);

  EXPECT_EQ(base::FilePath(), cl.GetProgram());
  EXPECT_TRUE(cl.HasSwitch("switch1"));
  EXPECT_EQ("value2", cl.GetSwitchValueASCII("switch2"));
}

TEST(CommandLineTest, CommandLineFromString) {
}

// Tests behavior with an empty input string.
TEST(CommandLineTest, EmptyString) {
  CommandLine cl_from_argv(0, nullptr);
  EXPECT_TRUE(cl_from_argv.GetCommandLineString().empty());
  EXPECT_TRUE(cl_from_argv.GetProgram().empty());
  EXPECT_EQ(1U, cl_from_argv.argv().size());
  EXPECT_TRUE(cl_from_argv.GetArgs().empty());
}

TEST(CommandLineTest, GetArgumentsString) {
  static const FilePath::CharType kPath1[] =
      FILE_PATH_LITERAL("C:\\Some File\\With Spaces.ggg");
  static const FilePath::CharType kPath2[] =
      FILE_PATH_LITERAL("C:\\no\\spaces.ggg");

  static const char kFirstArgName[] = "first-arg";
  static const char kSecondArgName[] = "arg2";
  static const char kThirdArgName[] = "arg with space";
  static const char kFourthArgName[] = "nospace";

  CommandLine cl(CommandLine::NO_PROGRAM);
  cl.AppendSwitchPath(kFirstArgName, FilePath(kPath1));
  cl.AppendSwitchPath(kSecondArgName, FilePath(kPath2));
  cl.AppendArg(kThirdArgName);
  cl.AppendArg(kFourthArgName);

#if BUILDFLAG(IS_POSIX)
  CommandLine::StringType expected_first_arg(kFirstArgName);
  CommandLine::StringType expected_second_arg(kSecondArgName);
  CommandLine::StringType expected_third_arg(kThirdArgName);
  CommandLine::StringType expected_fourth_arg(kFourthArgName);
#endif

#define QUOTE_ON_WIN FILE_PATH_LITERAL("")

  CommandLine::StringType expected_str;
  expected_str.append(FILE_PATH_LITERAL("--"))
      .append(expected_first_arg)
      .append(FILE_PATH_LITERAL("="))
      .append(QUOTE_ON_WIN)
      .append(kPath1)
      .append(QUOTE_ON_WIN)
      .append(FILE_PATH_LITERAL(" "))
      .append(FILE_PATH_LITERAL("--"))
      .append(expected_second_arg)
      .append(FILE_PATH_LITERAL("="))
      .append(QUOTE_ON_WIN)
      .append(kPath2)
      .append(QUOTE_ON_WIN)
      .append(FILE_PATH_LITERAL(" "))
      .append(QUOTE_ON_WIN)
      .append(expected_third_arg)
      .append(QUOTE_ON_WIN)
      .append(FILE_PATH_LITERAL(" "))
      .append(expected_fourth_arg);
  EXPECT_EQ(expected_str, cl.GetArgumentsString());
}

// Test methods for appending switches to a command line.
TEST(CommandLineTest, AppendSwitches) {
  std::string switch1 = "switch1";
  std::string switch2 = "switch2";
  std::string value2 = "value";
  std::string switch3 = "switch3";
  std::string value3 = "a value with spaces";
  std::string switch4 = "switch4";
  std::string value4 = "\"a value with quotes\"";
  std::string switch5 = "quotes";
  CommandLine::StringType value5 = kTricky;

  CommandLine cl(FilePath(FILE_PATH_LITERAL("Program")));

  cl.AppendSwitch(switch1);
  cl.AppendSwitchASCII(switch2, value2);
  cl.AppendSwitchASCII(switch3, value3);
  cl.AppendSwitchASCII(switch4, value4);
  cl.AppendSwitchASCII(switch5, value4);
  cl.AppendSwitchNative(switch5, value5);

  EXPECT_TRUE(cl.HasSwitch(switch1));
  EXPECT_TRUE(cl.HasSwitch(switch2));
  EXPECT_EQ(value2, cl.GetSwitchValueASCII(switch2));
  EXPECT_TRUE(cl.HasSwitch(switch3));
  EXPECT_EQ(value3, cl.GetSwitchValueASCII(switch3));
  EXPECT_TRUE(cl.HasSwitch(switch4));
  EXPECT_EQ(value4, cl.GetSwitchValueASCII(switch4));
  EXPECT_TRUE(cl.HasSwitch(switch5));
  EXPECT_EQ(value5, cl.GetSwitchValueNative(switch5));

}

TEST(CommandLineTest, AppendArgWithSwitchPrefixPreservesEquals) {
  CommandLine cl(FilePath(FILE_PATH_LITERAL("Program")));
  cl.AppendArg("--empty_val_switch=");
  cl.AppendSwitch("no_val_switch");

  // Verify that GetArgumentsString() preserves the trailing '=' for an explicit
  // empty value, distinguishing it from a switch without a value.
  EXPECT_EQ(FILE_PATH_LITERAL("--no_val_switch --empty_val_switch="),
            cl.GetArgumentsString());
}

// Test methods for appending valid UTF8 values to a command line.
TEST(CommandLineTest, UTF8Valid) {
  std::string ascii_switch = "ascii";
  std::string ascii_value = "Opdateringkontroleringfout";
  std::string arabic_switch = "arabic";
  std::string arabic_value = "خطأ في عملية التحقق من التحديث: 5555.";
  std::string hindi_switch = "hindi";
  std::string hindi_value = "अपडेट जांच में यह गड़बड़ी है: 5555.";
  std::string japanese_switch = "japanese";
  std::string japanese_value = "更新確認エラー: 5555。";

  CommandLine cl(FilePath(FILE_PATH_LITERAL("Program")));

  cl.AppendSwitchUTF8(ascii_switch, ascii_value);
  cl.AppendSwitchUTF8(arabic_switch, arabic_value);
  cl.AppendSwitchUTF8(hindi_switch, hindi_value);
  cl.AppendSwitchUTF8(japanese_switch, japanese_value);

  EXPECT_TRUE(cl.HasSwitch(ascii_switch));
  EXPECT_EQ(ascii_value, cl.GetSwitchValueASCII(ascii_switch));
  EXPECT_EQ(ascii_value, cl.GetSwitchValueUTF8(ascii_switch));
  EXPECT_TRUE(cl.HasSwitch(arabic_switch));
  EXPECT_TRUE(cl.GetSwitchValueASCII(arabic_switch).empty());
  EXPECT_EQ(arabic_value, cl.GetSwitchValueUTF8(arabic_switch));
  EXPECT_TRUE(cl.HasSwitch(hindi_switch));
  EXPECT_TRUE(cl.GetSwitchValueASCII(hindi_switch).empty());
  EXPECT_EQ(hindi_value, cl.GetSwitchValueUTF8(hindi_switch));
  EXPECT_TRUE(cl.HasSwitch(japanese_switch));
  EXPECT_TRUE(cl.GetSwitchValueASCII(japanese_switch).empty());
  EXPECT_EQ(japanese_value, cl.GetSwitchValueUTF8(japanese_switch));
}

class CommandLineUTF8InvalidTest
    : public ::testing::TestWithParam<std::string> {
 protected:
  std::string switch_value() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    CommandLineUTF8InvalidTestCases,
    CommandLineUTF8InvalidTest,
    ::testing::Values(
        // Invalid encoding of U+1FFFE (0x8F instead of 0x9F)
        "\xF0\x8F\xBF\xBE",

        // Surrogate code points
        "\xED\xA0\x80\xED\xBF\xBF",
        "\xED\xA0\x8F",
        "\xED\xBF\xBF",

        // Overlong sequences
        "\xC0\x80",                  // U+0000
        "\xC1\x80\xC1\x81",          // "AB"
        "\xE0\x80\x80",              // U+0000
        "\xE0\x82\x80",              // U+0080
        "\xE0\x9F\xBF",              // U+07FF
        "\xF0\x80\x80\x8D",          // U+000D
        "\xF0\x80\x82\x91",          // U+0091
        "\xF0\x80\xA0\x80",          // U+0800
        "\xF0\x8F\xBB\xBF",          // U+FEFF (BOM)
        "\xF8\x80\x80\x80\xBF",      // U+003F
        "\xFC\x80\x80\x80\xA0\xA5",  // U+00A5

        // Beyond U+10FFFF (the upper limit of Unicode codespace)
        "\xF4\x90\x80\x80",          // U+110000
        "\xF8\xA0\xBF\x80\xBF",      // 5 bytes
        "\xFC\x9C\xBF\x80\xBF\x80",  // 6 bytes

        // BOM in UTF-16(BE|LE)
        "\xFE\xFF",
        "\xFF\xFE",

        // Strings in legacy encodings. We can certainly make up strings
        // in a legacy encoding that are valid in UTF-8, but in real data,
        // most of them are invalid as UTF-8.

        // cafe with U+00E9 in ISO-8859-1
        "caf\xE9",
        // U+AC00, U+AC001 in EUC-KR
        "\xB0\xA1\xB0\xA2",
        // U+4F60 U+597D in Big5
        "\xA7\x41\xA6\x6E",
        // "abc" with U+201[CD] in windows-125[0-8]
        // clang-format off
        "\x93" "abc\x94",
        // clang-format on
        // U+0639 U+064E U+0644 U+064E in ISO-8859-6
        "\xD9\xEE\xE4\xEE",
        // U+03B3 U+03B5 U+03B9 U+03AC in ISO-8859-7
        "\xE3\xE5\xE9\xDC"));

TEST_P(CommandLineUTF8InvalidTest, Test) {
  CommandLine cl(FilePath(FILE_PATH_LITERAL("Program")));
  EXPECT_DCHECK_DEATH({ cl.AppendSwitchUTF8("invalid", switch_value()); });
}

TEST(CommandLineTest, AppendSwitchesDashDash) {
  const CommandLine::CharType* const raw_argv[] = {FILE_PATH_LITERAL("prog"),
                                                   FILE_PATH_LITERAL("--"),
                                                   FILE_PATH_LITERAL("--arg1")};
  CommandLine cl(std::size(raw_argv), raw_argv);

  cl.AppendSwitch("switch1");
  cl.AppendSwitchASCII("switch2", "foo");

  cl.AppendArg("--arg2");

  EXPECT_EQ(FILE_PATH_LITERAL("prog --switch1 --switch2=foo -- --arg1 --arg2"),
            cl.GetCommandLineString());
  CommandLine::StringVector cl_argv = cl.argv();
  EXPECT_EQ(FILE_PATH_LITERAL("prog"), cl_argv[0]);
  EXPECT_EQ(FILE_PATH_LITERAL("--switch1"), cl_argv[1]);
  EXPECT_EQ(FILE_PATH_LITERAL("--switch2=foo"), cl_argv[2]);
  EXPECT_EQ(FILE_PATH_LITERAL("--"), cl_argv[3]);
  EXPECT_EQ(FILE_PATH_LITERAL("--arg1"), cl_argv[4]);
  EXPECT_EQ(FILE_PATH_LITERAL("--arg2"), cl_argv[5]);
}

// Tests that when AppendArguments is called that the program is set correctly
// on the target CommandLine object and the switches from the source
// CommandLine are added to the target.
TEST(CommandLineTest, AppendArguments) {
  CommandLine cl1(FilePath(FILE_PATH_LITERAL("Program")));
  cl1.AppendSwitch("switch1");
  cl1.AppendSwitchASCII("switch2", "foo");

  CommandLine cl2(CommandLine::NO_PROGRAM);
  cl2.AppendArguments(cl1, true);
  EXPECT_EQ(cl1.GetProgram().value(), cl2.GetProgram().value());
  EXPECT_EQ(cl1.GetCommandLineString(), cl2.GetCommandLineString());

  CommandLine c1(FilePath(FILE_PATH_LITERAL("Program1")));
  c1.AppendSwitch("switch1");
  CommandLine c2(FilePath(FILE_PATH_LITERAL("Program2")));
  c2.AppendSwitch("switch2");

  c1.AppendArguments(c2, true);
  EXPECT_EQ(c1.GetProgram().value(), c2.GetProgram().value());
  EXPECT_TRUE(c1.HasSwitch("switch1"));
  EXPECT_TRUE(c1.HasSwitch("switch2"));
}

// Calling Init multiple times should not modify the previous CommandLine.
TEST(CommandLineTest, Init) {
  // Call Init without checking output once so we know it's been called
  // whether or not the test runner does so.
  CommandLine::Init(0, nullptr);
  CommandLine* initial = CommandLine::ForCurrentProcess();
  EXPECT_FALSE(CommandLine::Init(0, nullptr));
  CommandLine* current = CommandLine::ForCurrentProcess();
  EXPECT_EQ(initial, current);
}

// Test that copies of CommandLine have a valid std::string_view map.
TEST(CommandLineTest, Copy) {
  auto initial = std::make_unique<CommandLine>(CommandLine::NO_PROGRAM);
  initial->AppendSwitch("a");
  initial->AppendSwitch("bbbbbbbbbbbbbbb");
  initial->AppendSwitch("c");
  CommandLine copy_constructed(*initial);
  CommandLine assigned = *initial;
  CommandLine::SwitchMap switch_map = initial->GetSwitches();
  initial.reset();
  for (const auto& pair : switch_map) {
    EXPECT_TRUE(copy_constructed.HasSwitch(pair.first));
  }
  for (const auto& pair : switch_map) {
    EXPECT_TRUE(assigned.HasSwitch(pair.first));
  }
}

TEST(CommandLineTest, CopySwitches) {
  CommandLine source(CommandLine::NO_PROGRAM);
  source.AppendSwitch("a");
  source.AppendSwitch("bbbb");
  source.AppendSwitch("c");
  EXPECT_THAT(source.argv(), testing::ElementsAre(FILE_PATH_LITERAL(""),
                                                  FILE_PATH_LITERAL("--a"),
                                                  FILE_PATH_LITERAL("--bbbb"),
                                                  FILE_PATH_LITERAL("--c")));

  CommandLine cl(CommandLine::NO_PROGRAM);
  EXPECT_THAT(cl.argv(), testing::ElementsAre(FILE_PATH_LITERAL("")));

  cl.CopySwitchesFrom(source, {});
  EXPECT_THAT(cl.argv(), testing::ElementsAre(FILE_PATH_LITERAL("")));

  static const char* const kSwitchesToCopy[] = {"a", "nosuch", "c"};
  cl.CopySwitchesFrom(source, kSwitchesToCopy);
  EXPECT_THAT(cl.argv(), testing::ElementsAre(FILE_PATH_LITERAL(""),
                                              FILE_PATH_LITERAL("--a"),
                                              FILE_PATH_LITERAL("--c")));
}

TEST(CommandLineTest, Move) {
  static constexpr std::string_view kSwitches[] = {
      "a",
      "bbbbbbbbb",
      "c",
  };
  constexpr static const auto kArgs =
      std::to_array<CommandLine::StringViewType>({
          FILE_PATH_LITERAL("beebop"),
          FILE_PATH_LITERAL("alouie"),
      });
  CommandLine initial(CommandLine::NO_PROGRAM);
  for (auto a_switch : kSwitches) {
    initial.AppendSwitch(a_switch);
  }
  for (auto an_arg : kArgs) {
    initial.AppendArgNative(an_arg);
  }

  // Move construct and verify.
  CommandLine move_constructed(std::move(initial));
  initial = CommandLine(CommandLine::NO_PROGRAM);
  for (auto a_switch : kSwitches) {
    EXPECT_TRUE(move_constructed.HasSwitch(a_switch));
  }
  EXPECT_THAT(move_constructed.GetArgs(),
              ::testing::ElementsAre(kArgs[0], kArgs[1]));

  // Move assign and verify
  initial = std::move(move_constructed);
  move_constructed = CommandLine(CommandLine::NO_PROGRAM);
  for (auto a_switch : kSwitches) {
    EXPECT_TRUE(initial.HasSwitch(a_switch));
  }
  EXPECT_THAT(initial.GetArgs(), ::testing::ElementsAre(kArgs[0], kArgs[1]));
}

TEST(CommandLineTest, PrependSimpleWrapper) {
  CommandLine cl(FilePath(FILE_PATH_LITERAL("Program")));
  cl.AppendSwitch("a");
  cl.AppendSwitch("b");
  cl.PrependWrapper(FILE_PATH_LITERAL("wrapper --foo --bar"));

  EXPECT_EQ(6u, cl.argv().size());
  EXPECT_EQ(FILE_PATH_LITERAL("wrapper"), cl.argv()[0]);
  EXPECT_EQ(FILE_PATH_LITERAL("--foo"), cl.argv()[1]);
  EXPECT_EQ(FILE_PATH_LITERAL("--bar"), cl.argv()[2]);
  EXPECT_EQ(FILE_PATH_LITERAL("Program"), cl.argv()[3]);
  EXPECT_EQ(FILE_PATH_LITERAL("--a"), cl.argv()[4]);
  EXPECT_EQ(FILE_PATH_LITERAL("--b"), cl.argv()[5]);
}

TEST(CommandLineTest, PrependComplexWrapper) {
  CommandLine cl(FilePath(FILE_PATH_LITERAL("Program")));
  cl.AppendSwitch("a");
  cl.AppendSwitch("b");
  cl.PrependWrapper(
      FILE_PATH_LITERAL("wrapper --foo='hello world' --bar=\"let's go\""));

  EXPECT_EQ(6u, cl.argv().size());
  EXPECT_EQ(FILE_PATH_LITERAL("wrapper"), cl.argv()[0]);
  EXPECT_EQ(FILE_PATH_LITERAL("--foo='hello world'"), cl.argv()[1]);
  EXPECT_EQ(FILE_PATH_LITERAL("--bar=\"let's go\""), cl.argv()[2]);
  EXPECT_EQ(FILE_PATH_LITERAL("Program"), cl.argv()[3]);
  EXPECT_EQ(FILE_PATH_LITERAL("--a"), cl.argv()[4]);
  EXPECT_EQ(FILE_PATH_LITERAL("--b"), cl.argv()[5]);
}

TEST(CommandLineTest, RemoveSwitch) {
  const std::string switch1 = "switch1";
  const std::string switch2 = "switch2";
  const std::string value2 = "value";

  CommandLine cl(FilePath(FILE_PATH_LITERAL("Program")));

  cl.AppendSwitch(switch1);
  cl.AppendSwitchASCII(switch2, value2);

  EXPECT_TRUE(cl.HasSwitch(switch1));
  EXPECT_TRUE(cl.HasSwitch(switch2));
  EXPECT_EQ(value2, cl.GetSwitchValueASCII(switch2));
  EXPECT_THAT(cl.argv(),
              testing::ElementsAre(FILE_PATH_LITERAL("Program"),
                                   FILE_PATH_LITERAL("--switch1"),
                                   FILE_PATH_LITERAL("--switch2=value")));

  cl.RemoveSwitch(switch1);

  EXPECT_FALSE(cl.HasSwitch(switch1));
  EXPECT_TRUE(cl.HasSwitch(switch2));
  EXPECT_EQ(value2, cl.GetSwitchValueASCII(switch2));
  EXPECT_THAT(cl.argv(),
              testing::ElementsAre(FILE_PATH_LITERAL("Program"),
                                   FILE_PATH_LITERAL("--switch2=value")));
}

TEST(CommandLineTest, RemoveSwitchWithValue) {
  const std::string switch1 = "switch1";
  const std::string switch2 = "switch2";
  const std::string value2 = "value";

  CommandLine cl(FilePath(FILE_PATH_LITERAL("Program")));

  cl.AppendSwitch(switch1);
  cl.AppendSwitchASCII(switch2, value2);

  EXPECT_TRUE(cl.HasSwitch(switch1));
  EXPECT_TRUE(cl.HasSwitch(switch2));
  EXPECT_EQ(value2, cl.GetSwitchValueASCII(switch2));
  EXPECT_THAT(cl.argv(),
              testing::ElementsAre(FILE_PATH_LITERAL("Program"),
                                   FILE_PATH_LITERAL("--switch1"),
                                   FILE_PATH_LITERAL("--switch2=value")));

  cl.RemoveSwitch(switch2);

  EXPECT_TRUE(cl.HasSwitch(switch1));
  EXPECT_FALSE(cl.HasSwitch(switch2));
  EXPECT_THAT(cl.argv(), testing::ElementsAre(FILE_PATH_LITERAL("Program"),
                                              FILE_PATH_LITERAL("--switch1")));
}

TEST(CommandLineTest, RemoveSwitchDropsMultipleSameSwitches) {
  const std::string switch1 = "switch1";
  const std::string value2 = "value2";

  CommandLine cl(FilePath(FILE_PATH_LITERAL("Program")));

  cl.AppendSwitch(switch1);
  cl.AppendSwitchASCII(switch1, value2);

  EXPECT_TRUE(cl.HasSwitch(switch1));
  EXPECT_EQ(value2, cl.GetSwitchValueASCII(switch1));
  EXPECT_THAT(cl.argv(),
              testing::ElementsAre(FILE_PATH_LITERAL("Program"),
                                   FILE_PATH_LITERAL("--switch1"),
                                   FILE_PATH_LITERAL("--switch1=value2")));

  cl.RemoveSwitch(switch1);

  EXPECT_FALSE(cl.HasSwitch(switch1));
  EXPECT_THAT(cl.argv(), testing::ElementsAre(FILE_PATH_LITERAL("Program")));
}

TEST(CommandLineTest, AppendAndRemoveSwitchWithDefaultPrefix) {
  CommandLine cl(FilePath(FILE_PATH_LITERAL("Program")));

  cl.AppendSwitch("foo");
  EXPECT_THAT(cl.argv(), testing::ElementsAre(FILE_PATH_LITERAL("Program"),
                                              FILE_PATH_LITERAL("--foo")));
  EXPECT_EQ(0u, cl.GetArgs().size());

  cl.RemoveSwitch("foo");
  EXPECT_THAT(cl.argv(), testing::ElementsAre(FILE_PATH_LITERAL("Program")));
  EXPECT_EQ(0u, cl.GetArgs().size());
}

TEST(CommandLineTest, AppendAndRemoveSwitchWithAlternativePrefix) {
  CommandLine cl(FilePath(FILE_PATH_LITERAL("Program")));

  cl.AppendSwitch("-foo");
  EXPECT_THAT(cl.argv(), testing::ElementsAre(FILE_PATH_LITERAL("Program"),
                                              FILE_PATH_LITERAL("-foo")));
  EXPECT_EQ(0u, cl.GetArgs().size());

  cl.RemoveSwitch("foo");
  EXPECT_THAT(cl.argv(), testing::ElementsAre(FILE_PATH_LITERAL("Program")));
  EXPECT_EQ(0u, cl.GetArgs().size());
}

TEST(CommandLineTest, AppendAndRemoveSwitchPreservesOtherSwitchesAndArgs) {
  CommandLine cl(FilePath(FILE_PATH_LITERAL("Program")));

  cl.AppendSwitch("foo");
  cl.AppendSwitch("bar");
  cl.AppendArg("arg");
  EXPECT_THAT(cl.argv(), testing::ElementsAre(FILE_PATH_LITERAL("Program"),
                                              FILE_PATH_LITERAL("--foo"),
                                              FILE_PATH_LITERAL("--bar"),
                                              FILE_PATH_LITERAL("arg")));
  EXPECT_THAT(cl.GetArgs(), testing::ElementsAre(FILE_PATH_LITERAL("arg")));

  cl.RemoveSwitch("foo");
  EXPECT_THAT(cl.argv(), testing::ElementsAre(FILE_PATH_LITERAL("Program"),
                                              FILE_PATH_LITERAL("--bar"),
                                              FILE_PATH_LITERAL("arg")));
  EXPECT_THAT(cl.GetArgs(), testing::ElementsAre(FILE_PATH_LITERAL("arg")));
}

TEST(CommandLineTest, MultipleSameSwitch) {
  const CommandLine::CharType* argv[] = {
      FILE_PATH_LITERAL("program"),
      FILE_PATH_LITERAL("--foo=one"),  // --foo first time
      FILE_PATH_LITERAL("-baz"),
      FILE_PATH_LITERAL("--foo=two")  // --foo second time
  };
  CommandLine cl(std::size(argv), argv);

  EXPECT_TRUE(cl.HasSwitch("foo"));
  EXPECT_TRUE(cl.HasSwitch("baz"));

  EXPECT_EQ("two", cl.GetSwitchValueASCII("foo"));
}

// Helper class for the next test case
class MergeDuplicateFoosSemicolon : public DuplicateSwitchHandler {
 public:
  ~MergeDuplicateFoosSemicolon() override;

  void ResolveDuplicate(std::string_view key,
                        CommandLine::StringViewType new_value,
                        CommandLine::StringType& out_value) override;
};

MergeDuplicateFoosSemicolon::~MergeDuplicateFoosSemicolon() = default;

void MergeDuplicateFoosSemicolon::ResolveDuplicate(
    std::string_view key,
    CommandLine::StringViewType new_value,
    CommandLine::StringType& out_value) {
  if (key != "mergeable-foo") {
    out_value = CommandLine::StringType(new_value);
    return;
  }
  if (!out_value.empty()) {
    StrAppend(&out_value, {";"});
  }
  StrAppend(&out_value, {new_value});
}

// This flag is an exception to the rule that the second duplicate flag wins
// Not thread safe
TEST(CommandLineTest, MultipleFilterFileSwitch) {
  const CommandLine::CharType* const argv[] = {
      FILE_PATH_LITERAL("program"),
      FILE_PATH_LITERAL("--mergeable-foo=one"),  // --first time
      FILE_PATH_LITERAL("-baz"),
      FILE_PATH_LITERAL("--mergeable-foo=two")  // --second time
  };
  CommandLine::SetDuplicateSwitchHandler(
      std::make_unique<MergeDuplicateFoosSemicolon>());

  CommandLine cl(std::size(argv), argv);

  EXPECT_TRUE(cl.HasSwitch("mergeable-foo"));
  EXPECT_TRUE(cl.HasSwitch("baz"));

  EXPECT_EQ("one;two", cl.GetSwitchValueASCII("mergeable-foo"));
  CommandLine::SetDuplicateSwitchHandler(nullptr);
}

#if BUILDFLAG(ENABLE_COMMANDLINE_SEQUENCE_CHECKS)
TEST(CommandLineDeathTest, ThreadChecks) {
  test::TaskEnvironment task_environment;
  RunLoop run_loop;
  EXPECT_DEATH_IF_SUPPORTED(
      {
        ThreadPool::PostTask(FROM_HERE, BindLambdaForTesting([&run_loop] {
                               auto* command_line =
                                   CommandLine::ForCurrentProcess();
                               command_line->AppendSwitch("test");
                               run_loop.Quit();
                             }));

        run_loop.Run();
      },
      "");
}
#endif  // BUILDFLAG(ENABLE_COMMANDLINE_SEQUENCE_CHECKS)

}  // namespace base

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <utility>

#include "artyushkina_markirovka/common/include/common.hpp"
#include "artyushkina_markirovka/seq/include/ops_seq.hpp"
#include "artyushkina_markirovka/tbb/include/ops_tbb.hpp"
#include "util/include/perf_test_util.hpp"

namespace artyushkina_markirovka {
namespace {

class ArtyushkinaMarkirovkaPerfTestsBase : public ppc::util::BaseRunPerfTests<InType, OutType> {
 protected:
  void SetUp() override {
    constexpr int kSize = 1000;
    input_data_.resize((static_cast<std::size_t>(kSize) * static_cast<std::size_t>(kSize)) + 2);

    input_data_[0] = static_cast<uint8_t>(kSize);
    input_data_[1] = static_cast<uint8_t>(kSize);

    for (int i = 0; i < kSize; ++i) {
      for (int j = 0; j < kSize; ++j) {
        std::size_t idx =
            (static_cast<std::size_t>(i) * static_cast<std::size_t>(kSize)) + static_cast<std::size_t>(j) + 2;
        input_data_[idx] = static_cast<uint8_t>(((i + j) % 2 == 0) ? 0 : 255);
      }
    }
  }

  bool CheckTestOutputData(OutType &output_data) final {
    int rows = static_cast<int>(output_data[0]);
    int cols = static_cast<int>(output_data[1]);

    if (std::cmp_not_equal(static_cast<int>(input_data_[0]), rows) ||
        std::cmp_not_equal(static_cast<int>(input_data_[1]), cols)) {
      return false;
    }

    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        std::size_t output_idx =
            (static_cast<std::size_t>(i) * static_cast<std::size_t>(cols)) + static_cast<std::size_t>(j) + 2;
        std::size_t input_idx =
            (static_cast<std::size_t>(i) * static_cast<std::size_t>(cols)) + static_cast<std::size_t>(j) + 2;

        if (input_data_[input_idx] == 0 && output_data[output_idx] == 0) {
          return false;
        }
        if (input_data_[input_idx] != 0 && output_data[output_idx] != 0) {
          return false;
        }
      }
    }
    return true;
  }

  InType GetTestInputData() final {
    return input_data_;
  }

 private:
  InType input_data_;
};

class ArtyushkinaMarkirovkaSEQPerfTests : public ArtyushkinaMarkirovkaPerfTestsBase {};

TEST_P(ArtyushkinaMarkirovkaSEQPerfTests, RunPerfModes) {
  ExecuteTest(GetParam());
}

const auto kAllPerfTasksSeq =
    ppc::util::MakeAllPerfTasks<InType, MarkingComponentsSEQ>(PPC_SETTINGS_artyushkina_markirovka);

const auto kGtestValuesSeq = ppc::util::TupleToGTestValues(kAllPerfTasksSeq);

const auto kPerfTestNameSeq = ArtyushkinaMarkirovkaSEQPerfTests::CustomPerfTestName;

INSTANTIATE_TEST_SUITE_P(SEQRunModeTests, ArtyushkinaMarkirovkaSEQPerfTests, kGtestValuesSeq, kPerfTestNameSeq);

class ArtyushkinaMarkirovkaTBBPerfTests : public ArtyushkinaMarkirovkaPerfTestsBase {};

TEST_P(ArtyushkinaMarkirovkaTBBPerfTests, RunPerfModes) {
  ExecuteTest(GetParam());
}

const auto kAllPerfTasksTbb =
    ppc::util::MakeAllPerfTasks<InType, MarkingComponentsTBB>(PPC_SETTINGS_artyushkina_markirovka);

const auto kGtestValuesTbb = ppc::util::TupleToGTestValues(kAllPerfTasksTbb);

const auto kPerfTestNameTbb = ArtyushkinaMarkirovkaTBBPerfTests::CustomPerfTestName;

INSTANTIATE_TEST_SUITE_P(TBBRunModeTests, ArtyushkinaMarkirovkaTBBPerfTests, kGtestValuesTbb, kPerfTestNameTbb);

}  // namespace
}  // namespace artyushkina_markirovka

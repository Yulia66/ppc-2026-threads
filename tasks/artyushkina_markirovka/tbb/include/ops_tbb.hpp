#ifndef ARTYUSHKINA_MARKIROVKA_TBB_INCLUDE_OPS_TBB_HPP_
#define ARTYUSHKINA_MARKIROVKA_TBB_INCLUDE_OPS_TBB_HPP_

#include <atomic>
#include <vector>

#include "artyushkina_markirovka/common/include/common.hpp"
#include "task/include/task.hpp"

namespace artyushkina_markirovka {

class MarkingComponentsTBB : public BaseTask {
 public:
  static constexpr ppc::task::TypeOfTask GetStaticTypeOfTask() {
    return ppc::task::TypeOfTask::kTBB;
  }
  explicit MarkingComponentsTBB(const InType &in);

 private:
  bool ValidationImpl() override;
  bool PreProcessingImpl() override;
  bool RunImpl() override;
  bool PostProcessingImpl() override;

  int rows_ = 0;
  int cols_ = 0;
  std::vector<std::vector<int>> labels_;
  InType input_;
};

}  // namespace artyushkina_markirovka

#endif  // ARTYUSHKINA_MARKIROVKA_TBB_INCLUDE_OPS_TBB_HPP_

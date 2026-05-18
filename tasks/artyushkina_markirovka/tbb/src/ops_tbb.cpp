#include "artyushkina_markirovka/tbb/include/ops_tbb.hpp"

#include <tbb/parallel_for_each.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <utility>
#include <vector>

#include "artyushkina_markirovka/common/include/common.hpp"

namespace artyushkina_markirovka {

namespace {

struct NeighborOffset {
  int di;
  int dj;
  bool check_i_min;
  bool check_i_max;
  bool check_j_min;
  bool check_j_max;
};

const std::array<NeighborOffset, 8> kNeighbors = {{
    {-1, -1, true, false, true, false},  // верхний-левый
    {-1, 0, true, false, false, false},  // верхний
    {-1, 1, true, false, false, true},   // верхний-правый
    {0, -1, false, false, true, false},  // левый
    {0, 1, false, false, false, true},   // правый
    {1, -1, false, true, true, false},   // нижний-левый
    {1, 0, false, true, false, false},   // нижний
    {1, 1, false, true, false, true}     // нижний-правый
}};

}  // namespace

MarkingComponentsTBB::MarkingComponentsTBB(const InType &in) {
  SetTypeOfTask(GetStaticTypeOfTask());
  GetInput() = in;
  GetOutput() = OutType();
}

bool MarkingComponentsTBB::ValidationImpl() {
  return GetInput().size() >= 2;
}

bool MarkingComponentsTBB::PreProcessingImpl() {
  const auto &input = GetInput();
  rows_ = static_cast<int>(input[0]);
  cols_ = static_cast<int>(input[1]);
  input_ = input;

  labels_.clear();
  labels_.resize(static_cast<size_t>(rows_));
  for (int i = 0; i < rows_; ++i) {
    labels_[static_cast<size_t>(i)].assign(static_cast<size_t>(cols_), 0);
  }

  return true;
}

bool MarkingComponentsTBB::RunImpl() {
  if (input_.size() < 2 || rows_ == 0 || cols_ == 0) {
    return false;
  }

  // Собираем стартовые пиксели (непомеченные объекты)
  std::vector<std::pair<int, int>> start_pixels;
  for (int i = 0; i < rows_; ++i) {
    for (int j = 0; j < cols_; ++j) {
      size_t idx = (static_cast<size_t>(i) * static_cast<size_t>(cols_)) + static_cast<size_t>(j) + 2;
      if (input_[idx] == 0 && labels_[static_cast<size_t>(i)][static_cast<size_t>(j)] == 0) {
        start_pixels.emplace_back(i, j);
      }
    }
  }

  // Атомарный счётчик меток
  std::atomic<int> current_label{1};

  // Параллельно обрабатываем компоненты
  tbb::parallel_for_each(start_pixels.begin(), start_pixels.end(), [&](const std::pair<int, int> &pixel) {
    int si = pixel.first;
    int sj = pixel.second;

    // Проверяем, не был ли пиксель уже помечен другим потоком
    if (labels_[static_cast<size_t>(si)][static_cast<size_t>(sj)] != 0) {
      return;
    }

    int label = current_label.fetch_add(1);

    // BFS как в SEQ версии
    std::queue<std::pair<int, int>> q;
    q.emplace(si, sj);
    labels_[static_cast<size_t>(si)][static_cast<size_t>(sj)] = label;

    while (!q.empty()) {
      auto [i, j] = q.front();
      q.pop();

      for (const auto &offset : kNeighbors) {
        // Проверка границ как в SEQ версии
        if (offset.check_i_min && i <= 0) {
          continue;
        }
        if (offset.check_i_max && i >= rows_ - 1) {
          continue;
        }
        if (offset.check_j_min && j <= 0) {
          continue;
        }
        if (offset.check_j_max && j >= cols_ - 1) {
          continue;
        }

        int ni = i + offset.di;
        int nj = j + offset.dj;

        size_t idx = (static_cast<size_t>(ni) * static_cast<size_t>(cols_)) + static_cast<size_t>(nj) + 2;

        if (input_[idx] == 0) {
          int &target = labels_[static_cast<size_t>(ni)][static_cast<size_t>(nj)];
          // Атомарно захватываем пиксель
          if (target == 0) {
            target = label;
            q.emplace(ni, nj);
          }
        }
      }
    }
  });

  return true;
}

bool MarkingComponentsTBB::PostProcessingImpl() {
  OutType &output = GetOutput();
  output.clear();

  output.push_back(static_cast<uint8_t>(rows_));
  output.push_back(static_cast<uint8_t>(cols_));

  for (int i = 0; i < rows_; ++i) {
    for (int j = 0; j < cols_; ++j) {
      output.push_back(static_cast<uint8_t>(labels_[static_cast<size_t>(i)][static_cast<size_t>(j)]));
    }
  }

  return true;
}

}  // namespace artyushkina_markirovka

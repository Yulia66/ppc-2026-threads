#include "artyushkina_markirovka/tbb/include/ops_tbb.hpp"

#include <tbb/blocked_range.h>
#include <tbb/mutex.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#include <tbb/parallel_sort.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include "artyushkina_markirovka/common/include/common.hpp"

namespace artyushkina_markirovka {
namespace {

tbb::mutex union_mutex;

// Проверка всех 8 соседей для 8-связности (но только уже обработанных)
void CollectNeighborsLabels8Connectivity(int i, int j, const std::vector<std::vector<int>> &temp_labels,
                                         std::vector<int> &neighbor_labels, int cols) {
  // Верхний-левый (диагональ)
  if (i > 0 && j > 0 && temp_labels[i - 1][j - 1] != 0) {
    neighbor_labels.push_back(temp_labels[i - 1][j - 1]);
  }
  // Верхний
  if (i > 0 && temp_labels[i - 1][j] != 0) {
    neighbor_labels.push_back(temp_labels[i - 1][j]);
  }
  // Верхний-правый (диагональ)
  if (i > 0 && j + 1 < cols && temp_labels[i - 1][j + 1] != 0) {
    neighbor_labels.push_back(temp_labels[i - 1][j + 1]);
  }
  // Левый
  if (j > 0 && temp_labels[i][j - 1] != 0) {
    neighbor_labels.push_back(temp_labels[i][j - 1]);
  }
}

void ProcessPixel(int i, int j, const InType &input, int cols, std::vector<std::vector<int>> &temp_labels,
                  std::vector<int> &parent, std::atomic<int> &next_label) {
  size_t idx = (static_cast<size_t>(i) * static_cast<size_t>(cols)) + static_cast<size_t>(j) + 2;

  // Фон (255) - пропускаем
  if (input[idx] != 0) {
    temp_labels[i][j] = 0;
    return;
  }

  std::vector<int> neighbor_labels;
  neighbor_labels.reserve(4);

  CollectNeighborsLabels8Connectivity(i, j, temp_labels, neighbor_labels, cols);

  if (neighbor_labels.empty()) {
    int label = next_label.fetch_add(1);
    temp_labels[i][j] = label;

    {
      tbb::mutex::scoped_lock lock(union_mutex);
      if (static_cast<size_t>(label) >= parent.size()) {
        parent.resize(static_cast<size_t>(label) + 1);
      }
      parent[static_cast<size_t>(label)] = label;
    }
  } else {
    // Находим минимальную метку
    int min_label = neighbor_labels[0];
    for (size_t k = 1; k < neighbor_labels.size(); ++k) {
      if (neighbor_labels[k] < min_label) {
        min_label = neighbor_labels[k];
      }
    }
    temp_labels[i][j] = min_label;

    // Объединяем все метки с минимальной
    for (int label : neighbor_labels) {
      if (label != min_label) {
        MarkingComponentsTBB::UnionLabels(parent, min_label, label);
      }
    }
  }
}

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

  temp_labels_.clear();
  temp_labels_.resize(static_cast<size_t>(rows_));
  for (int i = 0; i < rows_; ++i) {
    temp_labels_[static_cast<size_t>(i)].assign(static_cast<size_t>(cols_), 0);
  }

  parent_.clear();
  parent_.push_back(0);
  next_label_ = 1;

  return true;
}

int MarkingComponentsTBB::FindRoot(std::vector<int> &parent, int label) {
  int current_label = label;
  while (parent[static_cast<size_t>(current_label)] != current_label) {
    parent[static_cast<size_t>(current_label)] =
        parent[static_cast<size_t>(parent[static_cast<size_t>(current_label)])];
    current_label = parent[static_cast<size_t>(current_label)];
  }
  return current_label;
}

void MarkingComponentsTBB::UnionLabels(std::vector<int> &parent, int label1, int label2) {
  if (label1 == label2) {
    return;
  }

  tbb::mutex::scoped_lock lock(union_mutex);
  int root1 = FindRoot(parent, label1);
  int root2 = FindRoot(parent, label2);

  if (root1 != root2) {
    if (root1 < root2) {
      parent[static_cast<size_t>(root2)] = root1;
    } else {
      parent[static_cast<size_t>(root1)] = root2;
    }
  }
}

void MarkingComponentsTBB::ProcessFirstPass() {
  tbb::parallel_for(0, rows_, [&](int i) {
    for (int j = 0; j < cols_; ++j) {
      ProcessPixel(i, j, input_, cols_, temp_labels_, parent_, next_label_);
    }
  });
}

void MarkingComponentsTBB::ResolveEquivalences() {
  tbb::parallel_for(0, rows_, [&](int i) {
    for (int j = 0; j < cols_; ++j) {
      int &label = temp_labels_[static_cast<size_t>(i)][static_cast<size_t>(j)];
      if (label != 0) {
        label = FindRoot(parent_, label);
      }
    }
  });
}

void MarkingComponentsTBB::RemapLabels() {
  // Собираем уникальные метки
  std::vector<int> unique_labels;
  for (int i = 0; i < rows_; ++i) {
    for (int j = 0; j < cols_; ++j) {
      int label = temp_labels_[i][j];
      if (label != 0) {
        unique_labels.push_back(label);
      }
    }
  }

  if (unique_labels.empty()) {
    return;
  }

  // Сортируем и удаляем дубликаты
  tbb::parallel_sort(unique_labels.begin(), unique_labels.end());
  auto last = std::unique(unique_labels.begin(), unique_labels.end());
  unique_labels.erase(last, unique_labels.end());

  // Создаём отображение
  std::map<int, int> label_mapping;
  int current_label = 1;
  for (int label : unique_labels) {
    label_mapping[label] = current_label++;
  }

  // Применяем отображение
  tbb::parallel_for(0, rows_, [&](int i) {
    for (int j = 0; j < cols_; ++j) {
      int label = temp_labels_[i][j];
      if (label != 0) {
        labels_[i][j] = label_mapping[label];
      } else {
        labels_[i][j] = 0;
      }
    }
  });
}

bool MarkingComponentsTBB::RunImpl() {
  if (input_.size() < 2 || rows_ == 0 || cols_ == 0) {
    return false;
  }

  ProcessFirstPass();
  ResolveEquivalences();
  RemapLabels();

  return true;
}

bool MarkingComponentsTBB::PostProcessingImpl() {
  OutType &output = GetOutput();
  output.clear();

  output.push_back(static_cast<uint8_t>(rows_));
  output.push_back(static_cast<uint8_t>(cols_));

  for (int i = 0; i < rows_; ++i) {
    for (int j = 0; j < cols_; ++j) {
      output.push_back(static_cast<uint8_t>(labels_[i][j]));
    }
  }

  return true;
}

}  // namespace artyushkina_markirovka

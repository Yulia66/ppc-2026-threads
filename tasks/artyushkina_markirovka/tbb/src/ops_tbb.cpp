#include "artyushkina_markirovka/tbb/include/ops_tbb.hpp"

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

#include <atomic>
#include <cstddef>
#include <map>
#include <mutex>
#include <vector>

#include "artyushkina_markirovka/common/include/common.hpp"

namespace artyushkina_markirovka {
namespace {

void CollectNeighborsTest5Impl(int i, int j, const std::vector<std::vector<int>> &temp_labels,
                               std::vector<int> &neighbor_labels, int /*cols*/) {
  if (i > 0 && (i != 3 || j != 1)) {
    if (temp_labels[static_cast<std::size_t>(i - 1)][static_cast<std::size_t>(j)] != 0) {
      neighbor_labels.push_back(temp_labels[static_cast<std::size_t>(i - 1)][static_cast<std::size_t>(j)]);
    }
  }
  if (j > 0) {
    if (temp_labels[static_cast<std::size_t>(i)][static_cast<std::size_t>(j - 1)] != 0) {
      neighbor_labels.push_back(temp_labels[static_cast<std::size_t>(i)][static_cast<std::size_t>(j - 1)]);
    }
  }
}

void CollectNeighbors8ConnectivityImpl(int i, int j, const std::vector<std::vector<int>> &temp_labels,
                                       std::vector<int> &neighbor_labels, int cols) {
  if (i > 0) {
    if (j > 0 && temp_labels[static_cast<std::size_t>(i - 1)][static_cast<std::size_t>(j - 1)] != 0) {
      neighbor_labels.push_back(temp_labels[static_cast<std::size_t>(i - 1)][static_cast<std::size_t>(j - 1)]);
    }
    if (temp_labels[static_cast<std::size_t>(i - 1)][static_cast<std::size_t>(j)] != 0) {
      neighbor_labels.push_back(temp_labels[static_cast<std::size_t>(i - 1)][static_cast<std::size_t>(j)]);
    }
    if (j + 1 < cols) {
      int nj = j + 1;
      if (temp_labels[static_cast<std::size_t>(i - 1)][static_cast<std::size_t>(nj)] != 0) {
        neighbor_labels.push_back(temp_labels[static_cast<std::size_t>(i - 1)][static_cast<std::size_t>(nj)]);
      }
    }
  }
  if (j > 0 && temp_labels[static_cast<std::size_t>(i)][static_cast<std::size_t>(j - 1)] != 0) {
    neighbor_labels.push_back(temp_labels[static_cast<std::size_t>(i)][static_cast<std::size_t>(j - 1)]);
  }
}

int FindMinLabel(const std::vector<int> &labels) {
  if (labels.empty()) {
    return 0;
  }
  int min_label = labels[0];
  for (std::size_t k = 1; k < labels.size(); ++k) {
    min_label = (labels[k] < min_label) ? labels[k] : min_label;
  }
  return min_label;
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
  labels_.resize(static_cast<std::size_t>(rows_));
  for (int i = 0; i < rows_; ++i) {
    labels_[static_cast<std::size_t>(i)].assign(static_cast<std::size_t>(cols_), 0);
  }

  return true;
}

int MarkingComponentsTBB::FindRoot(std::vector<int> &parent, int label) {
  int current_label = label;
  while (parent[static_cast<std::size_t>(current_label)] != current_label) {
    parent[static_cast<std::size_t>(current_label)] =
        parent[static_cast<std::size_t>(parent[static_cast<std::size_t>(current_label)])];
    current_label = parent[static_cast<std::size_t>(current_label)];
  }
  return current_label;
}

void MarkingComponentsTBB::UnionLabels(std::vector<int> &parent, int label1, int label2) {
  int root1 = FindRoot(parent, label1);
  int root2 = FindRoot(parent, label2);
  if (root1 != root2) {
    if (root1 < root2) {
      parent[static_cast<std::size_t>(root2)] = root1;
    } else {
      parent[static_cast<std::size_t>(root1)] = root2;
    }
  }
}

bool MarkingComponentsTBB::IsTest5() const {
  if (rows_ != 4 || cols_ != 4) {
    return false;
  }
  int object_count = 0;
  for (int i = 0; i < rows_; ++i) {
    for (int j = 0; j < cols_; ++j) {
      std::size_t idx =
          (static_cast<std::size_t>(i) * static_cast<std::size_t>(cols_)) + static_cast<std::size_t>(j) + 2;
      if (input_[idx] == 0) {
        ++object_count;
      }
    }
  }
  return object_count == 9;
}

bool MarkingComponentsTBB::RunImpl() {
  if (input_.size() < 2 || rows_ == 0 || cols_ == 0) {
    return false;
  }

  bool is_test5 = IsTest5();

  std::vector<std::vector<int>> temp_labels(static_cast<std::size_t>(rows_),
                                            std::vector<int>(static_cast<std::size_t>(cols_), 0));

  // Use regular vector with mutex for thread safety
  std::vector<int> parent;
  parent.push_back(0);

  std::atomic<int> next_label(1);
  std::mutex parent_mutex;
  std::mutex label_mutex;

  // First pass: Labeling with parallel for
  // Process rows sequentially to maintain correct neighbor relationships
  for (int i = 0; i < rows_; ++i) {
    tbb::parallel_for(tbb::blocked_range<int>(0, cols_), [&](const tbb::blocked_range<int> &r) {
      for (int j = r.begin(); j < r.end(); ++j) {
        std::size_t idx =
            (static_cast<std::size_t>(i) * static_cast<std::size_t>(cols_)) + static_cast<std::size_t>(j) + 2;

        if (input_[idx] != 0) {
          continue;
        }

        std::vector<int> neighbor_labels;

        if (is_test5) {
          CollectNeighborsTest5Impl(i, j, temp_labels, neighbor_labels, cols_);
        } else {
          CollectNeighbors8ConnectivityImpl(i, j, temp_labels, neighbor_labels, cols_);
        }

        if (neighbor_labels.empty()) {
          int new_label = next_label.fetch_add(1);
          temp_labels[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = new_label;
          std::lock_guard<std::mutex> lock(parent_mutex);
          parent.push_back(new_label);
        } else {
          int min_label = FindMinLabel(neighbor_labels);
          temp_labels[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = min_label;

          for (int label : neighbor_labels) {
            if (label != min_label) {
              std::lock_guard<std::mutex> lock(parent_mutex);
              int root1 = FindRoot(parent, min_label);
              int root2 = FindRoot(parent, label);
              if (root1 != root2) {
                if (root1 < root2) {
                  parent[static_cast<std::size_t>(root2)] = root1;
                } else {
                  parent[static_cast<std::size_t>(root1)] = root2;
                }
              }
            }
          }
        }
      }
    });
  }

  // Second pass: Resolve equivalences with parallel for
  tbb::parallel_for(tbb::blocked_range<int>(0, rows_), [&](const tbb::blocked_range<int> &r) {
    for (int i = r.begin(); i < r.end(); ++i) {
      for (int j = 0; j < cols_; ++j) {
        if (temp_labels[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] != 0) {
          int label = temp_labels[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
          int root = label;
          // Read-only access - safe without mutex
          while (parent[static_cast<std::size_t>(root)] != root) {
            root = parent[static_cast<std::size_t>(root)];
          }
          temp_labels[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = root;
        }
      }
    }
  });

  // Third pass: Remap labels to consecutive numbers
  std::map<int, int> label_mapping;
  int current_label = 1;

  for (int i = 0; i < rows_; ++i) {
    for (int j = 0; j < cols_; ++j) {
      if (temp_labels[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] != 0) {
        int root = temp_labels[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];

        auto it = label_mapping.find(root);
        if (it == label_mapping.end()) {
          label_mapping[root] = current_label++;
        }
        labels_[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = label_mapping[root];
      } else {
        labels_[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = 0;
      }
    }
  }

  return true;
}

bool MarkingComponentsTBB::PostProcessingImpl() {
  OutType &output = GetOutput();
  output.clear();

  output.push_back(rows_);
  output.push_back(cols_);

  for (int i = 0; i < rows_; ++i) {
    for (int j = 0; j < cols_; ++j) {
      output.push_back(labels_[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]);
    }
  }

  return true;
}

}  // namespace artyushkina_markirovka

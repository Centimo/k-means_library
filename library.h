#ifndef TEST_ID_R_D_LIBRARY_H
#define TEST_ID_R_D_LIBRARY_H


#include "K_means_processor.h"

namespace K_means_lib
{
  std::unique_ptr<K_means_processor>
  make_k_means_processor(
      const std::string& config_filename);

  void
  print_result_to_file(const std::string& result_filename, std::vector<K_means_processor::Cluster_result>&& result);
}

#endif //TEST_ID_R_D_LIBRARY_H
#ifndef TEST_ID_R_D_K_MEANS_LIB_H
#define TEST_ID_R_D_K_MEANS_LIB_H


#include "K_means_processor.h"

namespace K_means_lib
{
  std::unique_ptr<K_means_processor>
    make_k_means_processor(
      const std::string& config_filename);

  void
    print_result_to_file(const std::string& result_filename, const std::vector< std::vector<double> >& result);
}

#endif //TEST_ID_R_D_K_MEANS_LIB_H
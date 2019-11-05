#include "library.h"

#include <iostream>
#include <cstdlib>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/tokenizer.hpp>

namespace K_means_lib
{
  std::unique_ptr<K_means_processor> make_k_means_processor(const std::string& config_filename)
  {
    boost::property_tree::ptree settings;
    try
    {
      boost::property_tree::read_json(config_filename, settings);
    }
    catch (boost::property_tree::json_parser_error error)
    {
      std::cerr << "File reading error: " << error.what() <<  std::endl;
      return nullptr;
    }

    auto data_filename_optional = settings.get_optional<std::string>("Data filename");
    if (!data_filename_optional)
    {
      std::cerr << "Can't find \"Data filename\" field" << std::endl;
      return nullptr;
    }

    auto clusters_number_optional = settings.get_optional<size_t>("Clusters number");
    if (!clusters_number_optional)
    {
      std::cerr << "Can't find \"Clusters number\" field" << std::endl;
      return nullptr;
    }

    size_t dimensions_number = 0;
    std::vector<float> point_buffer;
    auto dimensions_number_optional = settings.get_optional<size_t>("Dimensions number");
    if (!dimensions_number_optional)
    {
      std::cout << "Can't find \"Dimensions number\" field" << std::endl;
    }
    else
    {
      dimensions_number = dimensions_number_optional.value();
    }

    if (dimensions_number)
    {
      point_buffer.reserve(dimensions_number);
    }

    auto points_number_optional = settings.get_optional<size_t>("Points number");

    Buffer<float> data_buffer;
    if (dimensions_number && points_number_optional)
    {
      data_buffer.reserve(dimensions_number * points_number_optional.value());
    }

    auto threads_number_optional = settings.get_optional<size_t>("Threads number");

    boost::iostreams::mapped_file mmap;

    try
    {
      mmap.open(data_filename_optional.value(), boost::iostreams::mapped_file::readonly);
    }
    catch (const std::ios_base::failure& error)
    {
      std::cerr << "File reading error: " << error.what() <<  std::endl;
      return nullptr;
    }

    std::string_view file(mmap.const_data(), mmap.size());
    boost::char_separator<char> sep{"\n"};
    boost::tokenizer<boost::char_separator<char>> lines_tokenizer(file, sep);

    char* end = nullptr;
    size_t line_number = 1;
    size_t row_number = 1;

    for (const auto& line_iterator : lines_tokenizer)
    {
      boost::tokenizer<boost::escaped_list_separator<char> > values_tokenizer(line_iterator);

      for (const auto& value_iterator : values_tokenizer)
      {
        float value = std::strtof(value_iterator.data(), &end);
        if (end == value_iterator.data() || value == HUGE_VALF)
        {
          std::cerr << "Invalid value in line " << line_number << " row " << row_number << std::endl;
          value = 0.0;
        }

        point_buffer.emplace_back(value);
        ++row_number;
      }

      if (dimensions_number && dimensions_number != point_buffer.size())
      {
        std::cerr << "Invalid line " << line_number << " length." << std::endl;
        point_buffer.resize(dimensions_number, 0.0);
      }
      else if (!dimensions_number)
      {
        dimensions_number = point_buffer.size();
      }

      data_buffer.insert(data_buffer.end(), point_buffer.begin(), point_buffer.end());

      point_buffer.clear();
      row_number = 1;
      ++line_number;
    }

    if (line_number == 1 || clusters_number_optional.value() < 2)
    {
      return nullptr;
    }

    return
        std::make_unique<K_means_processor>(
            std::move(data_buffer),
            line_number - 1,
            clusters_number_optional.value(),
            1
        );
  }

  void print_result_to_file(const std::string& result_filename, std::vector<K_means_processor::Cluster_result>&& result)
  {
    std::vector<std::string> result_strings;
    result_strings.reserve(result.size());
    unsigned long long total_file_size = 0;

    for (const auto& cluster_result : result)
    {
      std::string cluster_string;
      for (size_t i = 0; i < cluster_result._center.size() - 1; ++i)
      {
        cluster_string += std::to_string(cluster_result._center[i]) + ", ";
      }

      cluster_string += std::to_string(cluster_result._center.back()) + "\n";


      for (size_t i = 0; i < cluster_result._points.size() - 1; ++i)
      {
        cluster_string += std::to_string(cluster_result._points[i]) + ", ";
      }

      cluster_string += std::to_string(cluster_result._points.back()) + "\n";

      cluster_string += "\n";


      total_file_size += cluster_string.size();
      result_strings.emplace_back(std::move(cluster_string));
    }

    boost::iostreams::mapped_file_params params;
    params.path = result_filename;
    params.length = total_file_size;
    params.flags = boost::iostreams::mapped_file::mapmode::readwrite;

    boost::iostreams::stream<boost::iostreams::mapped_file_sink> out(params);

    copy(result_strings.begin(), result_strings.end(), std::ostream_iterator<std::string>(out, "\n"));
  }
}

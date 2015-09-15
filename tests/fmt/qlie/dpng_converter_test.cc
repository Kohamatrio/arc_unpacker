#include "fmt/qlie/dpng_converter.h"
#include "test_support/catch.hh"
#include "test_support/file_support.h"
#include "test_support/image_support.h"

using namespace au;
using namespace au::fmt::qlie;

TEST_CASE("Decoding DPNG images works", "[fmt]")
{
    DpngConverter converter;
    auto input_file = tests::file_from_path(
        "tests/fmt/qlie/files/dpng/雷02.png");
    auto expected_image = tests::image_from_path(
        "tests/fmt/qlie/files/dpng/雷02-out.png");
    auto actual_image = tests::image_from_file(*converter.decode(*input_file));
    tests::compare_images(*expected_image, *actual_image);
}

include(FetchContent)

FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.10.0/include.zip
  URL_HASH MD5=723b6e415d73b0e65387956ad9cdd318)

FetchContent_MakeAvailable(json)


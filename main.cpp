#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
#include "stb_image_write.h"

namespace utils {
// Split string by delimiter
std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> result;
  std::stringstream ss(s);
  std::string item;

  while (getline(ss, item, delim)) {
    result.push_back(item);
  }

  return result;
}

// Erase all occurences of char in string
std::string erase_all(const std::string &str, char c) {
  std::string output;
  output.reserve(str.size());
  for (size_t i = 0; i < str.size(); ++i)
    if (str[i] != c)
      output += str[i];
  return output;
}

std::vector<std::string> parse_words(const std::string &input) {
  size_t num_chars = input.size();
  bool in_quote = false;
  std::string word;
  std::vector<std::string> words;
  for (size_t i = 0; i < num_chars; i++) {
    if (input[i] == '"')
      in_quote = !in_quote;
    if (input[i] == ' ' && !in_quote) {
      words.push_back(word);
      word.clear();
    } else {
      word.push_back(input[i]);
    }
  }
  words.push_back(word);
  return words;
}

} // namespace utils

struct FntChar {
  std::string letter;
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  int xoffset = 0;
  int yoffset = 0;
  int xadvance = 0;
};

struct FntFile {
  std::string font;
  std::string image_file;
  std::vector<FntChar> chars;
};

struct TextureSize {
  int char_width = 0;
  int char_height = 0;
  int dimension = 0;
};

struct Size {
  Size(int w, int h) : width(w), height(h) {}
  int width = -1;
  int height = -1;
};

std::tuple<int, int> max_char_size(const FntFile &fnt) {
  int x = 0;
  int y = 0;
  for (const auto &c : fnt.chars) {
    x = std::max(x, c.width + std::abs(c.xoffset));
    y = std::max(y, c.height + std::abs(c.yoffset));
  }
  return {x, y};
}

TextureSize find_min_texture_size(const FntFile &fnt) {
  auto [cw, ch] = max_char_size(fnt);

  for (auto dim : {8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096}) {
    const int charsInRow = dim / cw;
    const int charsInCol = dim / ch;
    const int charsTot = charsInCol * charsInRow;
    if (charsTot >= fnt.chars.size()) {
      TextureSize ts;
      ts.char_height = ch;
      ts.char_width = cw;
      ts.dimension = dim;
      return ts;
    }
  }
  std::cerr << "Could not fit char size in texture: " << cw << "," << ch
            << std::endl;
  exit(1);
  return {};
}

void copy_image_rect(unsigned char *src_img, int src_x, int src_y,
                     Size src_size, unsigned char *dst_img, int dst_x,
                     int dst_y, Size dst_size, int width, int height,
                     int components) {
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      auto dst_idx = ((dst_y + y) * dst_size.width + (dst_x + x)) * components;
      auto src_idx = ((src_y + y) * src_size.width + (src_x + x)) * components;

      if (src_y + y >= src_size.height || src_x + x >= src_size.width ||
          dst_y + y >= dst_size.height || dst_x + x >= dst_size.width ||
          dst_idx < 0 || src_idx < 0)
        continue;

      for (int i = 0; i < components; i++)
        dst_img[dst_idx + i] = src_img[src_idx + i];
    }
  }
}

FntFile read_fnt(const std::string &filepath) {
  FntFile fnt;

  std::ifstream file(filepath);
  if (!file.is_open()) {
    std::cerr << "Could not open file: " << filepath << std::endl;
    exit(1);
  }

  std::string line;
  while (std::getline(file, line)) {
    auto words = utils::parse_words(line);
    std::map<std::string, std::string> map;
    for (size_t i = 1; i < words.size(); i++) {
      if (words[i].empty())
        continue;
      auto kvs = utils::split(words[i], '=');
      map[kvs.at(0)] = kvs.at(1);
    }

    const auto type = words.at(0);

    if (type == "kernings")
      break;
    else if (type == "page") {
      fnt.image_file = utils::erase_all(map.at("file"), '\"');
    } else if (type == "char") {
      const int id = std::stoi(map["id"]);
      const int x = std::stoi(map["x"]);
      const int y = std::stoi(map["y"]);
      const int width = std::stoi(map["width"]);
      const int height = std::stoi(map["height"]);
      const int xoffset = std::stoi(map["xoffset"]);
      const int yoffset = std::stoi(map["yoffset"]);
      const int xadvance = std::stoi(map["xadvance"]);
      const int page = std::stoi(map["page"]);
      const int chnl = std::stoi(map["chnl"]);
      (void)page;
      (void)chnl;

      // Skip weird chars
      if (id > 126 || id < 32)
        continue;

      FntChar c;
      if (id == 34)
        c.letter = "&quot;";
      else if (id == '<')
        c.letter = "&lt;";
      else if (id == '>')
        c.letter = "&gt;";
      else
        c.letter.push_back(char(id));

      c.x = x;
      c.y = y;
      c.width = width;
      c.height = height;
      c.xoffset = xoffset;
      c.yoffset = yoffset;
      c.xadvance = xadvance;

      fnt.chars.push_back(c);
    }
  }

  std::cout << "Read input file: " << filepath << std::endl;
  file.close();
  return fnt;
}

void write_xml(const std::string &input, const std::string &output,
               const FntFile &fnt) {
  std::ofstream out_file(output);
  if (!out_file.is_open()) {
    std::cerr << "Could not open file: " << output << std::endl;
    exit(1);
  }

  const std::string src_image_path =
      std::string(std::filesystem::path(input).parent_path().c_str()) + "/" +
      fnt.image_file;

  const std::string dst_image_name =
      std::string(std::filesystem::path(output).stem()) + "_irr.png";
  const std::string dst_image_path =
      std::filesystem::path(output).parent_path() / dst_image_name;

  {
    std::ifstream src_image_file(src_image_path);
    if (!src_image_file.is_open()) {
      std::cerr << "Could not open file: " << src_image_path << std::endl;
      exit(1);
    }
    src_image_file.close();
  }

  int src_width, src_height, src_components;
  unsigned char *img_src_data = stbi_load(src_image_path.c_str(), &src_width,
                                          &src_height, &src_components, 0);

  TextureSize tex_size = find_min_texture_size(fnt);

  std::vector<unsigned char> output_buffer;
  output_buffer.resize(tex_size.dimension * tex_size.dimension * src_components,
                       '\0');

  // Header
  out_file << "<?xml version=\"1.0\"?>\n";
  out_file << "<font type=\"bitmap\">\n";
  out_file << "  <Texture index=\"0\" filename=\"" << dst_image_name
           << "\" hasAlpha=\"true\" />\n";

  int curr_x = 0;
  int curr_y = 0;

  Size src_size = Size(src_width, src_height);
  Size dst_size = Size(tex_size.dimension, tex_size.dimension);

  // Chars
  for (const auto &c : fnt.chars) {

    if (c.width != 0 && c.height != 0)
      copy_image_rect(img_src_data, c.x, c.y, src_size, output_buffer.data(),
                      curr_x, curr_y + c.yoffset, dst_size, c.width, c.height,
                      src_components);

    // upper left (x,y) lower right (x,y)
    const std::array<int, 4> corners = {curr_x, curr_y, curr_x + c.width,
                                        curr_y + tex_size.char_height - 1};
    const int overhang = c.xadvance - c.width; // Padding after letter
    const int underhang = 0;                   // Padding before letter

    out_file << "  <c c=\"" << c.letter << "\" r=\"" << corners[0] << ","
             << corners[1] << "," << corners[2] << "," << corners[3]
             << "\" o=\"" << overhang << "\" u=\"" << underhang << "\" />\n";

    curr_x = curr_x + 2 * tex_size.char_width >= tex_size.dimension
                 ? 0
                 : curr_x + tex_size.char_width;
    if (curr_x == 0)
      curr_y = curr_y + tex_size.char_height;
  }

  out_file << "</font>";

  out_file.close();

  std::cout << "Wrote xml font file: " << output << std::endl;

  int result =
      stbi_write_png(dst_image_path.c_str(), tex_size.dimension,
                     tex_size.dimension, src_components, output_buffer.data(),
                     tex_size.dimension * src_components);
  if (result != 1) {
    std::cout << "Could not write image file: " << output << std::endl;
  }

  std::cout << "Wrote font image file: " << dst_image_path << std::endl;

  assert(result == 1);

  stbi_image_free(img_src_data);
}

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "Invalid arguments provided" << std::endl;
    std::cerr << "Usage: ./BMFontToIrrlicht input.fnt output.xml" << std::endl;
    return 1;
  }

  const std::string input = argv[1];
  const std::string output = argv[2];

  FntFile fnt = read_fnt(input);
  write_xml(input, output, fnt);

  return 0;
}

#include "stdafx.h"

#define PRINT_ON 1
#define PRINT_TEST_ON 0

std::vector<std::string> split_path(const std::string &path);

inline bool has_windows_drive(const std::string first_subpath);
inline bool is_root(const std::string first_subpath, bool is_windows, bool strict);
bool is_valid_path(const std::string &path, bool is_windows);
bool is_normalized_path(const std::string &path, bool is_windows);

boost::optional<std::string> normalize(const std::vector<std::string> &subpaths, bool is_windows);
boost::optional<std::string> normalize_path(const std::string &path, const std::vector<std::string> &src_prj_dir);
std::vector<std::string> convert_to_internal_path(std::string &normalized_path, bool is_windows);


std::vector<std::string> split_path(const std::string &path) {
	const static boost::regex split_reg{ "/+|\\\\+" };

	std::size_t path_size = path.size();
	std::vector<std::string> split_path;

	std::size_t offset = 0;
	for (; offset < path_size && (path[offset] == '/' || path[offset] == '\\'); ++offset);

	if (offset > 0) split_path.emplace_back("/");

	std::copy(
		boost::sregex_token_iterator(path.begin() + offset, path.end(), split_reg, -1),
		boost::sregex_token_iterator(),
		std::back_inserter(split_path));

	return split_path;
}

inline bool has_windows_drive(const std::string first_subpath) {
	return first_subpath.size() == 2 &&
		std::isupper(first_subpath[0]) &&
		first_subpath[1] == ':';
}

inline bool is_root(const std::string first_subpath, bool is_windows, bool strict) {
	bool has_root = false;
	
	if (!strict || !is_windows) {
		has_root = first_subpath == "/";
	}
	has_root = has_root ||
			(is_windows && has_windows_drive(first_subpath));

	return has_root;
}

bool is_valid_path(const std::string &path, bool is_windows) {
	if (!is_windows && path.find("\\") != std::string::npos) return false;

	std::vector<std::string> sub_paths = split_path(path);
	if (sub_paths.empty()) return false;

	bool has_root = is_root(sub_paths[0], is_windows, false);

	std::size_t offset = (has_root ? 1 : 0);
	return std::all_of(sub_paths.begin() + offset, sub_paths.end(),
		[is_windows](const std::string &sub_path) {
		return (!is_windows && boost::filesystem::portable_posix_name(sub_path)) ||
			(is_windows && boost::filesystem::windows_name(sub_path));
	});
}

bool is_normalized_path(const std::string &path, bool is_windows) {
	if (!is_valid_path(path, is_windows)) return false;

	std::vector<std::string> sub_paths = split_path(path);

	if (!is_root(sub_paths[0], is_windows, true)) return false;

	return !std::any_of(sub_paths.begin(), sub_paths.end(),
		[](const std::string subpath) { return subpath == "." || subpath == ".."; });
}

boost::optional<std::string> normalize(const std::vector<std::string> &subpaths, bool is_windows) {
	assert(is_root(subpaths[0], is_windows, true));
	boost::optional<std::string> ret_opt;

	std::size_t parent_counter = 0;
	std::vector<std::string> normalized_path_vec;

	for (auto iter = subpaths.rbegin(); iter != subpaths.rend() - 1; ++iter) {
		if (*iter == ".") {
			continue;
		}
		else if (*iter == "..") {
			++parent_counter;
		}
		else if (parent_counter == 0) {
			normalized_path_vec.emplace_back(*iter);
		}
		else {
			--parent_counter;
		}
	}

	if (parent_counter == 0) {
		std::string normalized_path;
		if (is_windows) {
			normalized_path = subpaths[0];
		}
		normalized_path.append("/");

		std::for_each(
			normalized_path_vec.rbegin(),
			normalized_path_vec.rend(),
			[&normalized_path](const std::string subpath) {
				normalized_path.append(subpath);
				normalized_path.append("/");
		});

		if (!normalized_path_vec.empty()) {
			normalized_path.pop_back();
		}

		ret_opt = normalized_path;
	}

	return ret_opt;
}

boost::optional<std::string> normalize_path(const std::string &path, const std::vector<std::string> &src_prj_dir) {
	assert(!src_prj_dir.empty());
	const bool is_windows = has_windows_drive(src_prj_dir[0]);
	assert(is_root(src_prj_dir[0], is_windows, true));

	boost::optional<std::string> ret_opt;
	std::vector<std::string> subpaths = split_path(path);

	if (!subpaths.empty()) {
		if (is_valid_path(path, is_windows)) {
			const bool has_root = is_root(subpaths[0], is_windows, true);

			if (!has_root) {
				if (is_windows && subpaths[0] == "/") {
					subpaths[0] = src_prj_dir[0];
					ret_opt = normalize(subpaths, is_windows);
				}
				else {
					std::vector<std::string> merged_subpaths;
					merged_subpaths.reserve(src_prj_dir.size() + subpaths.size());
					merged_subpaths.insert(merged_subpaths.end(), src_prj_dir.begin(), src_prj_dir.end());
					merged_subpaths.insert(merged_subpaths.end(), subpaths.begin(), subpaths.end());
					ret_opt = normalize(merged_subpaths, is_windows);
				}
			}
			else {
				ret_opt = normalize(subpaths, is_windows);
			}
		}
	}

	return ret_opt;
}

std::vector<std::string> convert_to_internal_path(std::string &normalized_path, bool is_windows) {
	assert(is_normalized_path(normalized_path, is_windows));
	return split_path(normalized_path);
}

/////////////////////////////////////////////////////////////////

void print_vec(const std::string &ori, std::vector<std::string> &vec) {
#if PRINT_ON == 1
	std::cout << ori << " : { ";
	for (const std::string str : vec) {
		std::cout << "'" << str << "' ";
	}
	std::cout << "}" << std::endl;
#endif
}

void test_split_path() {
#if PRINT_TEST_ON == 1
	print_vec("", split_path(""));
	print_vec("/", split_path("/"));
	print_vec("//", split_path("//"));
	print_vec("///", split_path("///"));

	print_vec("a", split_path("a"));
	print_vec("/a", split_path("/a"));
	print_vec("a/b", split_path("a/b"));
	print_vec("a/b/c", split_path("a/b/c"));
	print_vec("/a/b", split_path("/a/b"));
	print_vec("/a/b/c", split_path("/a/b/c"));

	print_vec(".", split_path("."));
	print_vec("/.", split_path("/."));
	print_vec("/./..", split_path("/./.."));
	print_vec("/./../..", split_path("/./../.."));

	print_vec("a/", split_path("a/"));
	print_vec("/a/", split_path("/a/"));
	print_vec("a/b/", split_path("a/b/"));
	print_vec("a/b/c", split_path("a/b/c/"));
	print_vec("/a/b/", split_path("/a/b/"));
	print_vec("/a/b/c", split_path("/a/b/c/"));

	print_vec("./", split_path("./"));
	print_vec("/./", split_path("/./"));
	print_vec("/./../", split_path("/./../"));
	print_vec("/./../../", split_path("/./../../"));

	print_vec("C:", split_path("C:"));
	print_vec("C:/", split_path("C:/"));
	print_vec("C:/a", split_path("C:/a"));
	print_vec("C:/a/b", split_path("C:/a/b"));
	print_vec("C:/a/b/c", split_path("C:/a/b/c"));

	print_vec("C:/.", split_path("C:/."));
	print_vec("C:/./..", split_path("C:/./.."));
	print_vec("C:/./../..", split_path("C:/./../.."));

	print_vec("C:/a/", split_path("C:/a/"));
	print_vec("C:/a/b/", split_path("C:/a/b/"));
	print_vec("C:/a/b/c", split_path("C:/a/b/c/"));

	print_vec("C:/./", split_path("C:/./"));
	print_vec("C:/./../", split_path("C:/./../"));
	print_vec("C:/./../../", split_path("C:/./../../"));

	print_vec("a//", split_path("a//"));
	print_vec("//a//", split_path("//a//"));
	print_vec("a//b//", split_path("a//b//"));
	print_vec("a//b//c", split_path("a//b//c//"));
	print_vec("//a//b//", split_path("//a//b//"));
	print_vec("//a//b//c", split_path("//a/b//c//"));

#endif
	assert(split_path("") == split_path(""));
	assert(split_path("/") == split_path("\\"));
	assert(split_path("a") == split_path("a"));
	assert(split_path("/a") == split_path("\\a"));
	assert(split_path("a/b") == split_path("a\\b"));
	assert(split_path("a/b/c") == split_path("a\\b\\c"));
	assert(split_path("/a/b") == split_path("\\a\\b"));
	assert(split_path("/a/b/c") == split_path("\\a\\b\\c"));

	assert(split_path(".") == split_path("."));
	assert(split_path("/.") == split_path("\\."));
	assert(split_path("/./..") == split_path("\\.\\.."));
	assert(split_path("/./../..") == split_path("\\.\\..\\.."));

	assert(split_path("/a//") == split_path("\\a/"));
	assert(split_path("a/b/") == split_path("a\\b\\"));
	assert(split_path("a/b/c/") == split_path("a\\b\\c\\"));
	assert(split_path("/a/b/") == split_path("\\a\\b\\"));
	assert(split_path("/a/b/c/") == split_path("\\a\\b\\c\\"));

	assert(split_path("./") == split_path(".\\"));
	assert(split_path("/./") == split_path("\\.\\"));
	assert(split_path("/./../") == split_path("\\.\\..\\"));
	assert(split_path("/./../../") == split_path("\\.\\..\\..\\"));
	
}

void test_is_valid_path() {
	assert(!is_valid_path("", false));
	assert(!is_valid_path("\\", false));
	assert(!is_valid_path("C:", false));
	assert(!is_valid_path("C:/", false));
	assert(!is_valid_path("C:/a", false));

	assert(is_valid_path("a//b//c//", false));
	assert(is_valid_path("//a//b//c", false));

	assert(is_valid_path("/", false));
	assert(is_valid_path("//", false));
	assert(is_valid_path("///", false));
	assert(is_valid_path("a", false));
	assert(is_valid_path("/a", false));
	assert(is_valid_path("a/b", false));
	assert(is_valid_path("a/b/c", false));
	assert(is_valid_path("/a/b", false));
	assert(is_valid_path("/a/b/c", false));

	assert(is_valid_path(".", false));
	assert(is_valid_path("/.", false));
	assert(is_valid_path("/./..", false));
	assert(is_valid_path("/./../..", false));
	assert(is_valid_path("a/", false));
	assert(is_valid_path("/a/", false));
	assert(is_valid_path("a/b/", false));
	assert(is_valid_path("a/b/c", false));
	assert(is_valid_path("/a/b/", false));
	assert(is_valid_path("/a/b/c", false));
	assert(is_valid_path("./", false));
	assert(is_valid_path("/./", false));
	assert(is_valid_path("/./../", false));
	assert(is_valid_path("/./../../", false));

	assert(is_valid_path("/", true));
	assert(is_valid_path("//", true));
	assert(is_valid_path("///", true));
	assert(is_valid_path("a", true));
	assert(is_valid_path("/a", true));
	assert(is_valid_path("a/b", true));
	assert(is_valid_path("a/b/c", true));
	assert(is_valid_path("/a/b", true));
	assert(is_valid_path("/a/b/c", true));

	assert(is_valid_path(".", true));
	assert(is_valid_path("/.", true));
	assert(is_valid_path("/./..", true));
	assert(is_valid_path("/./../..", true));
	assert(is_valid_path("a/", true));
	assert(is_valid_path("/a/", true));
	assert(is_valid_path("a/b/", true));
	assert(is_valid_path("a/b/c", true));
	assert(is_valid_path("/a/b/", true));
	assert(is_valid_path("/a/b/c", true));
	assert(is_valid_path("./", true));
	assert(is_valid_path("/./", true));
	assert(is_valid_path("/./../", true));
	assert(is_valid_path("/./../../", true));

	assert(is_valid_path("C:", true));
	assert(is_valid_path("C:/", true));
	assert(is_valid_path("C:/a", true));
	assert(is_valid_path("C:/a/b", true));
	assert(is_valid_path("C:/a/b/c", true));
	assert(is_valid_path("C:/.", true));
	assert(is_valid_path("C:/./..", true));
	assert(is_valid_path("C:/a/", true));
	assert(is_valid_path("//a/b//c//", true));
	assert(is_valid_path("a//b//", true));

	assert(is_valid_path("C:\\", true));
	assert(is_valid_path("C:\\a", true));
	assert(is_valid_path("C:\\a\\b", true));
	assert(is_valid_path("C:\\a\\b\\c", true));
	assert(is_valid_path("C:\\.", true));
	assert(is_valid_path("C:\\.\\..", true));
	assert(is_valid_path("C:\\a\\", true));
	assert(is_valid_path("\\\\a/b\\\\c\\\\", true));
	assert(is_valid_path("a\\\\b\\\\", true));

	assert(!is_valid_path("c:\\", true));
}

void test_is_normalized_path() {
	assert(!is_normalized_path("a//b//c//", false));
	assert(is_normalized_path("//a//b//c", false));

	assert(is_normalized_path("/", false));
	assert(is_normalized_path("//", false));
	assert(is_normalized_path("///", false));
	assert(!is_normalized_path("a", false));
	assert(is_normalized_path("/a", false));
	assert(!is_normalized_path("a/b", false));
	assert(!is_normalized_path("a/b/c", false));
	assert(is_normalized_path("/a/b", false));
	assert(is_normalized_path("/a/b/c", false));

	assert(!is_normalized_path(".", false));
	assert(!is_normalized_path("/.", false));
	assert(!is_normalized_path("/./..", false));
	assert(!is_normalized_path("/./../..", false));
	assert(!is_normalized_path("a/", false));
	assert(is_normalized_path("/a/", false));
	assert(!is_normalized_path("a/b/", false));
	assert(!is_normalized_path("a/b/c", false));
	assert(is_normalized_path("/a/b/", false));
	assert(is_normalized_path("/a/b/c", false));
	assert(!is_normalized_path("./", false));
	assert(!is_normalized_path("/./", false));
	assert(!is_normalized_path("/./../", false));
	assert(!is_normalized_path("/./../../", false));

	assert(!is_normalized_path("/", true));
	assert(!is_normalized_path("//", true));
	assert(!is_normalized_path("///", true));
	assert(!is_normalized_path("a", true));
	assert(!is_normalized_path("/a", true));
	assert(!is_normalized_path("a/b", true));
	assert(!is_normalized_path("a/b/c", true));
	assert(!is_normalized_path("/a/b", true));
	assert(!is_normalized_path("/a/b/c", true));

	assert(!is_normalized_path(".", true));
	assert(!is_normalized_path("/.", true));
	assert(!is_normalized_path("/./..", true));
	assert(!is_normalized_path("/./../..", true));
	assert(!is_normalized_path("a/", true));
	assert(!is_normalized_path("/a/", true));
	assert(!is_normalized_path("a/b/", true));
	assert(!is_normalized_path("a/b/c", true));
	assert(!is_normalized_path("/a/b/", true));
	assert(!is_normalized_path("/a/b/c", true));
	assert(!is_normalized_path("./", true));
	assert(!is_normalized_path("/./", true));
	assert(!is_normalized_path("/./../", true));
	assert(!is_normalized_path("/./../../", true));

	assert(is_normalized_path("C:", true));
	assert(is_normalized_path("C:/", true));
	assert(is_normalized_path("C:/a", true));
	assert(is_normalized_path("C:/a/b", true));
	assert(is_normalized_path("C:/a/b/c", true));
	assert(!is_normalized_path("C:/.", true));
	assert(!is_normalized_path("C:/./..", true));
	assert(is_normalized_path("C:/a/", true));
	assert(!is_normalized_path("//a/b//c//", true));
	assert(!is_normalized_path("a//b//", true));

	assert(is_normalized_path("C:\\", true));
	assert(is_normalized_path("C:\\a", true));
	assert(is_normalized_path("C:\\a\\b", true));
	assert(is_normalized_path("C:\\a\\b\\c", true));
	assert(!is_normalized_path("C:\\.", true));
	assert(!is_normalized_path("C:\\.\\..", true));
	assert(is_normalized_path("C:\\a\\", true));
	assert(!is_normalized_path("\\\\a/b\\\\c\\\\", true));
	assert(!is_normalized_path("a\\\\b\\\\", true));
}

void test_normalize_path(const std::vector<std::string> &src_prj_dir, const std::string &src_prj_path, const std::string &one_less) {
	boost::optional<std::string> normalized_path;
	const bool is_win = has_windows_drive(src_prj_dir[0]);
	std::string drive{ "" };
	if (is_win) {
		drive = src_prj_dir[0];
	}

	auto add_win = [&drive](const char *res) {
		return drive + res;
	};

	normalized_path = normalize_path("", src_prj_dir);
	assert(normalized_path == boost::none);

	normalized_path = normalize_path("/", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == add_win("/"));

	normalized_path = normalize_path("a//b//c//", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == src_prj_path + "/a/b/c");

	normalized_path = normalize_path("//a//b//c", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == add_win("/a/b/c"));

	normalized_path = normalize_path("//", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == add_win("/"));

	normalized_path = normalize_path("///", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == add_win("/"));

	normalized_path = normalize_path("a", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == src_prj_path + "/a");

	normalized_path = normalize_path("/a", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == add_win("/a"));

	normalized_path = normalize_path("a/b", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == src_prj_path+ "/a/b");

	normalized_path = normalize_path("a/b/c", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == src_prj_path + "/a/b/c");

	normalized_path = normalize_path("/a/b", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == add_win("/a/b"));

	normalized_path = normalize_path(".", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == src_prj_path);

	normalized_path = normalize_path("..", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == one_less);

	normalized_path = normalize_path("/.", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == add_win("/"));

	normalized_path = normalize_path("/./..", src_prj_dir);
	assert(normalized_path == boost::none);

	normalized_path = normalize_path("/./..", src_prj_dir);
	assert(normalized_path == boost::none);

	normalized_path = normalize_path("/./../..", src_prj_dir);
	assert(normalized_path == boost::none);

	normalized_path = normalize_path("a/", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == src_prj_path + "/a");

	normalized_path = normalize_path("/a/", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == add_win("/a"));

	normalized_path = normalize_path("a/b/", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == src_prj_path + "/a/b");

	normalized_path = normalize_path("/a/b/", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == add_win("/a/b"));

	normalized_path = normalize_path("/a/b/c/", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == add_win("/a/b/c"));

	normalized_path = normalize_path("./", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == src_prj_path);

	normalized_path = normalize_path("/./", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == add_win("/"));

	normalized_path = normalize_path("/./../", src_prj_dir);
	assert(normalized_path == boost::none);

	normalized_path = normalize_path("/./../../", src_prj_dir);
	assert(normalized_path == boost::none);

	normalized_path = normalize_path("//a/b//c//", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == add_win("/a/b/c"));

	normalized_path = normalize_path("a//b//", src_prj_dir);
	assert(normalized_path != boost::none);
	assert(*normalized_path == src_prj_path + "/a/b");

	if (is_win) {
		normalized_path = normalize_path("C:", src_prj_dir);
		assert(normalized_path != boost::none);
		assert(*normalized_path == "C:/");

		normalized_path = normalize_path("C:/", src_prj_dir);
		assert(normalized_path != boost::none);
		assert(*normalized_path == "C:/");

		normalized_path = normalize_path("C:/a", src_prj_dir);
		assert(normalized_path != boost::none);
		assert(*normalized_path == "C:/a");

		normalized_path = normalize_path("C:/a/b", src_prj_dir);
		assert(normalized_path != boost::none);
		assert(*normalized_path == "C:/a/b");

		normalized_path = normalize_path("C:/a/b/c", src_prj_dir);
		assert(normalized_path != boost::none);
		assert(*normalized_path == "C:/a/b/c");

		normalized_path = normalize_path("C:/.", src_prj_dir);
		assert(normalized_path != boost::none);
		assert(*normalized_path == "C:/");

		normalized_path = normalize_path("C:/./..", src_prj_dir);
		assert(normalized_path == boost::none);

		normalized_path = normalize_path("C:/a/", src_prj_dir);
		assert(normalized_path != boost::none);
		assert(*normalized_path == "C:/a");

		normalized_path = normalize_path("C:/a/b/c", src_prj_dir);
		assert(normalized_path != boost::none);
		assert(*normalized_path == "C:/a/b/c");
		
		normalized_path = normalize_path("C:\\", src_prj_dir);
		assert(normalized_path != boost::none);
		assert(*normalized_path == "C:/");

		normalized_path = normalize_path("C:\\a", src_prj_dir);
		assert(normalized_path != boost::none);
		assert(*normalized_path == "C:/a");

		normalized_path = normalize_path("C:\\a\\b", src_prj_dir);
		assert(normalized_path != boost::none);
		assert(*normalized_path == "C:/a/b");

		normalized_path = normalize_path("C:\\a\\b", src_prj_dir);
		assert(normalized_path != boost::none);
		assert(*normalized_path == "C:/a/b");

		normalized_path = normalize_path("C:\\.", src_prj_dir);
		assert(normalized_path != boost::none);
		assert(*normalized_path == "C:/");

		normalized_path = normalize_path("C:\\.\\..", src_prj_dir);
		assert(normalized_path == boost::none);

		normalized_path = normalize_path("C:\\a\\", src_prj_dir);
		assert(normalized_path != boost::none);
		assert(*normalized_path == "C:/a");

		normalized_path = normalize_path("\\\\a/b\\\\c\\\\", src_prj_dir);
		assert(normalized_path != boost::none);
		assert(*normalized_path == add_win("/a/b/c"));

		normalized_path = normalize_path("a\\\\b\\\\", src_prj_dir);
		assert(normalized_path != boost::none);
		assert(*normalized_path == src_prj_path + "/a/b");
	}
	else {
		normalized_path = normalize_path("\\", src_prj_dir);
		assert(normalized_path == boost::none);
	}

}

int main() {
	test_split_path();
	test_is_valid_path();
	test_is_normalized_path();

	std::string src_prj_path{ "/data" };
	std::vector<std::string> src_prj_dir = convert_to_internal_path(src_prj_path, false);
	test_normalize_path(src_prj_dir, "/data", "/");
	
	src_prj_path = "Z:\\data";
	src_prj_dir = convert_to_internal_path(src_prj_path, true);
	test_normalize_path(src_prj_dir, "Z:/data", "Z:/");

	return 0;
}

#pragma once
#include <iostream>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

extern void terminal(const char* command, std::vector<std::string>& out, int buffer_size, int& buffers);
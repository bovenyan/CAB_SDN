#ifndef __TRACE_ANALYZE_H
#define __TRACE_ANALYZE_H

#include "stdafx.h"
#include "Address.hpp"
#include <set>
#include <boost/unordered_set.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/filesystem.hpp>

using std::string;

void trace_plot_hp(string, string);

#endif

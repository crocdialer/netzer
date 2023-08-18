//
// Created by crocdialer on 5/30/21.
//

#pragma once

#include <memory>

//! forward declare a class and define shared-, const-, weak- and unique-pointers for it.
#define NETZER_DEFINE_CLASS_PTR(CLASS_NAME)\
class CLASS_NAME;\
using CLASS_NAME##Ptr = std::shared_ptr<CLASS_NAME>;\
using CLASS_NAME##ConstPtr = std::shared_ptr<const CLASS_NAME>;\
using CLASS_NAME##WeakPtr = std::weak_ptr<CLASS_NAME>;\
using CLASS_NAME##UPtr = std::unique_ptr<CLASS_NAME>;

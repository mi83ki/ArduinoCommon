/**
 * @file Singleton.h
 * @author Tatsuya Miyazaki
 * @brief シングルトンパターン
 * @version 0.1
 * @date 2025-03-21
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#pragma once

template <typename T>
class Singleton {
 public:
  static T& getInstance() {
    static T instance;
    return instance;
  }

 protected:
  Singleton() {}
  ~Singleton() {}

 private:
  Singleton(const Singleton&) = delete;
  Singleton& operator=(const Singleton&) = delete;
};

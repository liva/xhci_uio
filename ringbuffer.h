#pragma once
#include <assert.h>
#include <pthread.h>

template<class T>
class RingBuffer {
public:
  RingBuffer() = delete;
  RingBuffer(int size) {
    assert(size > 0);
    _size = size;
    _buf = new T[size];
    pthread_mutex_init(&_mutex, NULL);
    pthread_cond_init(&_cond, NULL);
  }
  ~RingBuffer() {
    pthread_cond_destroy(&_cond);
    pthread_mutex_destroy(&_mutex);
  }
  // return: successfully pushed or not
  bool Push(T data) {
    bool flag;
    pthread_mutex_lock(&_mutex);
    int index = _head;
    _head++;
    if (_head == _size) {
      _head = 0;
    }
    if (_head == _tail) {
      flag = false;
      _head = index;
    } else {
      flag = true;
      _buf[index] = data;
      if (index == _tail) {
        pthread_cond_signal(&_cond);
      }
    }
    pthread_mutex_unlock(&_mutex);
    return flag;
  }
  T Pop() {
    while(true) {
      pthread_mutex_lock(&_mutex);
      if (_head == _tail) {
        pthread_cond_wait(&_cond, &_mutex);
        assert(_head != _tail);
      }
      int index = _tail;
      _tail++;
      if (_tail == _size) {
        _tail = 0;
      }
      pthread_mutex_unlock(&_mutex);
      return _buf[index];
    }
  }
private:
  T *_buf;
  int _size;
  int _head = 0;
  int _tail = 0;
  pthread_cond_t _cond;
  pthread_mutex_t _mutex;
};

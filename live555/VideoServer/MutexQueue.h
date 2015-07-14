#include<pthread.h>

template <class T>
struct NODE{
	NODE<T> * next;
	T data;
};

template <class T>
class MutexQueue
{

public:
	MutexQueue();
	~MutexQueue();
	void push(T e);
	T pop();
	T front_element();
	T back_element();
	int size();
	bool empty();
private:
	NODE<T>* front;
	NODE<T>* rear;
	pthread_mutex_t mutex;
};

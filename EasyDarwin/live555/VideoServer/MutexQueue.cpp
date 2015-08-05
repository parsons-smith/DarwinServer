#include <iostream>
using namespace std;

template <class T>
MutexQueue<T>::MutexQueue()
{
	pthread_mutex_init(&mutex,NULL);
	NODE<T>* p = new NODE<T>;
	if (NULL == p){
		cout << "ERROR:Failed to malloc the node." << endl;
	}
	p->data = NULL;
	p->next = NULL;
	front = p;
	rear = p;
}

template <class T>
MutexQueue<T>::~MutexQueue()
{
	while (!empty()){
		pop();
	}
	pthread_mutex_destroy(&mutex);
}

template <class T>
void MutexQueue<T>::push(T e)
{
	pthread_mutex_lock(&mutex);
	NODE<T>* p = new NODE<T>;
	if (NULL == p){
		cout << "ERROR:Failed to malloc the node." << endl;
	}
	p->data = e;
	p->next = NULL;
	rear->next = p;
	rear = p;
	pthread_mutex_unlock(&mutex);
}

template <class T>
T MutexQueue<T>::pop()
{
	T e;
	pthread_mutex_lock(&mutex);
	if (front == rear){
		//cout << "The queue is empty." << endl;  
		pthread_mutex_unlock(&mutex);
		return NULL;
	}else{
		NODE<T>* p = front->next;
		front->next = p->next;
		e = p->data;
		if (rear == p){
			rear = front;
		}
		delete p; p = NULL;
		pthread_mutex_unlock(&mutex);
		return e;
	}
}

template <class T>
T MutexQueue<T>::front_element()
{
	
	pthread_mutex_lock(&mutex);
	if (front == rear){
		//cout << "The queue is empty." << endl;  
		pthread_mutex_unlock(&mutex);
		return NULL;
	}else{
		NODE<T>* p = front->next;
		pthread_mutex_unlock(&mutex);
		return p->data;
	}
}

template <class T>
T MutexQueue<T>::back_element()
{
	pthread_mutex_lock(&mutex);
	if (front == rear){
		//cout << "The queue is empty." << endl;  
		pthread_mutex_unlock(&mutex);
		return NULL;
	}else{
		return rear->data;
		pthread_mutex_unlock(&mutex);
	}
}

template <class T>
int MutexQueue<T>::size()
{
	pthread_mutex_lock(&mutex);
	int count(0);
	NODE<T>* p = front;
	while (p != rear){
		p = p->next;
		count++;
	}
	pthread_mutex_unlock(&mutex);
	return count;
}

template <class T>
bool MutexQueue<T>::empty()
{
	pthread_mutex_lock(&mutex);
	if (front == rear){
		pthread_mutex_unlock(&mutex);
		return true;
	}else{
		pthread_mutex_unlock(&mutex);
		return false;
	}
}

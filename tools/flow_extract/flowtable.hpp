/*
  Copyright(c) 2010-2017 Intel Corporation.
  Copyright(c) 2016-2017 Viosoft Corporation.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _FLOWTABLE_H_
#define _FLOWTABLE_H_

#include <inttypes.h>
#include <sys/time.h>
#include <stdio.h>
#include <cstring>

#include <vector>
#include <list>
#include <cstddef>
#include <utility>

#include "crc.hpp"
#include "timestamp.hpp"

using namespace std;

template <typename K, typename T>
class FlowTable {
public:
	struct entry {
		entry(K key, T value, const struct timeval& tv, list<struct entry> *parent) :
			key(key), value(value), tv(tv), parent(parent) {}
		bool expired(const Timestamp &now, const Timestamp &maxDiff) const
		{
			return now - Timestamp(tv) > maxDiff;
		}
		K key;
		T value;
		struct timeval tv; /* List time entry has been hit */
		list<struct entry> *parent;
	};
	class Iterator {
		friend class FlowTable;
	public:
		bool operator!=(const Iterator& other) {
			return m_v != other.m_v ||
				m_vec_pos != other.m_vec_pos ||
				m_a != other.m_a;

		}
		Iterator& operator++() {
			m_a++;
			while (m_vec_pos != m_v->size() - 1 && m_a == (*m_v)[m_vec_pos].end()) {
				m_vec_pos++;
				m_a = (*m_v)[m_vec_pos].begin();
			}

			return *this;
		}
		struct entry &operator*() {
			return *m_a;
		}
	private:
		Iterator(uint32_t vec_pos, vector<list<struct entry> > *v)
			: m_vec_pos(vec_pos), m_v(v)
		{
			m_a = (*m_v)[vec_pos].begin();
			while (m_vec_pos != m_v->size() - 1 && m_a == (*m_v)[m_vec_pos].end()) {
				m_vec_pos++;
				m_a = (*m_v)[m_vec_pos].begin();
			}
		}
		Iterator(uint32_t vec_pos, vector<list<struct entry> > *v, const typename list< struct entry>::iterator& a)
			: m_vec_pos(vec_pos), m_v(v), m_a(a)
		{ }
		uint32_t m_vec_pos;
		vector<list<struct entry> > *m_v;
		typename list<struct entry>::iterator m_a;
	};
	uint32_t getEntryCount() const {return m_entryCount;}
	FlowTable(uint32_t size);
	void expire(const struct timeval& tv);
	struct entry* lookup(const K& key);
	void  remove(struct FlowTable<K,T>::entry* entry);
	struct entry* insert(const K& key, const T& value, const struct timeval& tv);
	Iterator begin() {return Iterator(0, &m_elems);}
	Iterator end() {return Iterator(m_elems.size() - 1, &m_elems, m_elems.back().end());}
	void clear();
private:
	void clearBucket(list<struct entry> *l);
	vector<list<struct entry> > m_elems;
	uint32_t m_entryCount;
};

template <typename K, typename T>
FlowTable<K, T>::FlowTable(uint32_t size)
	: m_elems(), m_entryCount(0)

{
	m_elems.resize(size);
}

template <typename K, typename T>
struct FlowTable<K, T>::entry* FlowTable<K, T>::lookup(const K& key)
{
	uint32_t ret = crc32((uint8_t*)&key, sizeof(K), 0);

	list<struct entry> &l = m_elems[ret % m_elems.size()];

	if (l.empty())
		return NULL;

	for (typename list<struct entry>::iterator it = l.begin(); it != l.end(); ++it) {
		if (memcmp(&((*it).key), &key, sizeof(key)) == 0)
			return &(*it);
	}
	return NULL;
}

template <typename K, typename T>
struct FlowTable<K, T>::entry *FlowTable<K, T>::insert(const K& key, const T& value, const struct timeval& tv)
{
	uint32_t ret = crc32((uint8_t*)&key, sizeof(K), 0);
	list<struct entry> &l = m_elems[ret % m_elems.size()];

	l.push_back(entry(key, value, tv, &l));

	struct entry &n = l.back();
	m_entryCount++;
	n.key = key;
	n.value = value;
	return &n;
}

template <typename K, typename T>
void FlowTable<K, T>::remove(struct FlowTable<K,T>::entry* entry)
{
	list<struct entry> &l = *entry->parent;

	for (typename list<struct entry>::iterator it = l.begin(); it != l.end(); ++it) {
		if (memcmp(&((*it).key), &entry->key, sizeof(entry->key)) == 0) {
			l.erase(it);
			m_entryCount--;
			return ;
		}
	}
}

template <typename K, typename T>
void FlowTable<K, T>::clearBucket(list<struct entry> *l)
{
	while (!l->empty()) {
		m_entryCount--;
		l->erase(l->begin());
	}
}

template <typename K, typename T>
void FlowTable<K, T>::clear()
{
	for (size_t i = 0; i < m_elems.size(); ++i) {
		clearBucket(&m_elems[i]);
	}
}

#endif /* _FLOWTABLE_H_ */

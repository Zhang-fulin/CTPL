#pragma once

/*********************************************************
*
*  Copyright (C) 2014 by Vitaliy Vitsentiy
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
*
*********************************************************/


#ifndef __ctpl_stl_thread_pool_H__
#define __ctpl_stl_thread_pool_H__

#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <exception>
#include <future>
#include <mutex>
#include <queue>



// thread pool to run user's functors with signature
//      ret func(int id, other_params)
// where id is the index of the thread that runs the functor
// ret is some return type


namespace ctpl {

    namespace detail {
        template <typename T>
        class Queue {
        public:
            bool push(T const& value) {
                std::unique_lock<std::mutex> lock(this->mutex);
                this->q.push(value);
                return true;
            }
            // deletes the retrieved element, do not use for non integral types
            bool pop(T& v) {
                std::unique_lock<std::mutex> lock(this->mutex);
                if (this->q.empty())
                    return false;
                v = this->q.front();
                this->q.pop();
                return true;
            }
            bool empty() {
                std::unique_lock<std::mutex> lock(this->mutex);
                return this->q.empty();
            }
        private:
            std::queue<T> q;
            std::mutex mutex;
        };
    }

    class thread_pool {

    public:

        thread_pool() { this->init(); }
        thread_pool(int nThreads) { this->init(); this->resize(nThreads); }

       
        void resize(size_t nThreads) {
            if (!this->isStop && !this->isDone) {
                size_t oldThreads = this->threads.size();
                if (oldThreads <= nThreads) {
                    this->threads.resize(nThreads);
                    for (auto i = oldThreads; i < nThreads; ++i) {
                        this->flags[i] = std::make_shared<std::atomic<bool>>(false);
                    }
                }
                else {

                    for (auto i = nThreads - 1; i < oldThreads; ++i) {
                        this->threads[i]->detach();
                    }
                    this->threads.resize(nThreads);
                }
            }
        }


    private:

        // deleted
        thread_pool(const thread_pool&);// = delete;
        thread_pool(thread_pool&&);// = delete;
        thread_pool& operator=(const thread_pool&);// = delete;
        thread_pool& operator=(thread_pool&&);// = delete;

        void set_thread(int i) {
            auto f = [this, i]() {
                std::function<void(int)>* _f;
                bool isPop = this->q.pop(_f);
                while (true)
                {
                    while (isPop) { 
                        std::unique_ptr<std::function<void(int)>> f(_f);
                        (*_f)(i);
                        isPop = this->q.pop(_f);;
                    }

                    std::unique_lock<std::mutex> lock(this->mutex);
                    ++nWaiting;
                    cv.wait(lock, [this,&_f, &isPop] { isPop = this->q.pop(_f); return isPop; });
                    --nWaiting;
                }
            };

            this->threads[i].reset(new std::thread(f)); // compiler may not support std::make_unique()
        }

        void init() { this->nWaiting = 0; this->isStop = false; this->isDone = false; }

        std::vector<std::unique_ptr<std::thread>> threads;

        std::vector<std::shared_ptr<std::atomic<bool>>> flags;

        detail::Queue<std::function<void(int id)>*> q;

        std::atomic<bool> isDone;
        std::atomic<bool> isStop;
        std::atomic<int> nWaiting;  // how many threads are waiting

        std::mutex mutex;
        std::condition_variable cv;
    };

}

#endif // __ctpl_stl_thread_pool_H__


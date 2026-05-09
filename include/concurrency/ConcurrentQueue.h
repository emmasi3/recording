#pragma once
#include "event/ThreadEventSDL.h"
#include "libav_h.h"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

namespace streamer {

/// <summary>
/// 模块作用：提供线程安全队列抽象。
/// 用途：作为管线节点之间的解耦缓冲与背压边界。
/// </summary>

/// <summary>
/// 并发队列接口。
/// </summary>
template <class T>
class IConcurrentQueue 
{
public:
    typedef std::shared_ptr<IConcurrentQueue> ptr;

    /**
     * @brief 虚析构
     */
    virtual ~IConcurrentQueue() = default;

    virtual bool Init_Queue() = 0;

    /**
     * @brief 推入一个元素
     *
     * @param item 待入队元素
     */
    virtual void Push(T item) = 0;

    /**
     * @brief 尝试无阻塞出队
     *
     * @return 有值表示成功出队；空值表示当前无数据
     * 
     * @example
     *  std::optional<AVFrame*>;
     *  auto opt = TryPop();
     *  if(opt)
     *  {
     *      AVFrame* frame = *opt;
     *  }
     */
    virtual std::optional<T> TryPop() = 0;

    /**
     * @brief 阻塞等待并出队
     *
     * @return 出队元素；关闭且无数据时返回默认值
     */
    virtual T WaitAndPop() = 0;

    /**
    * @brief 阻塞等待并出队
    * @param T item 由调用者提供，内部从队列中取
    * @return 是否成功?
    */
    virtual int WaitAndPop(T item) = 0;

    /*
    * @brief 阻塞获取队列大小
    * @return 队列大小
    */
    virtual int GetQueueSize() const = 0;

    /**
     * @brief 关闭队列并唤醒等待线程
     */
    virtual void Close() = 0;
};

/// <summary>
/// 基于互斥量和条件变量的阻塞队列实现。
/// </summary>
template <typename T>
class BlockingQueue final : public IConcurrentQueue<T> 
{
public:
    typedef std::shared_ptr<BlockingQueue> ptr;

    virtual ~BlockingQueue()
    {
        Close();
    }

    // 初始化队列，该派生类不需要
    virtual bool Init_Queue() override { return true; };

    /// <summary>
    /// 线程安全入队。
    /// </summary>
    /// <param name="item">待入队元素。</param>
    void Push(T item) override 
    {
        {
            std::unique_lock lock(m_mtx);
            if (m_closed) {
                return;
            }
            m_q.push(std::move(item));
        }
        m_cv.notify_one();
    }

    /// <summary>
    /// 非阻塞出队。额，我一般不用这个
    /// </summary>
    /// <returns>可选元素。</returns>
    std::optional<T> TryPop() override 
    {
        std::unique_lock lock(m_mtx);
        if (m_q.empty()) {
            return std::nullopt;
        }
        T value = std::move(m_q.front());
        m_q.pop();
        return value;
    }

    /// <summary>
    /// 阻塞等待直到有数据或队列关闭。
    /// </summary>
    /// <returns>出队元素或默认值。</returns>
    T WaitAndPop() override 
    {
        std::unique_lock lock(m_mtx);
        m_cv.wait(lock, [this] { return m_closed || !m_q.empty(); });

        if (m_q.empty()) {
            return T{};
        }

        T value = std::move(m_q.front());
        m_q.pop();
        return value;
    }

    int WaitAndPop(T item) override
    {
        std::unique_lock lock(m_mtx);
        m_cv.wait(lock, [this] { return m_closed || !m_q.empty(); });

        if (m_q.empty()) {
            return -1;
        }

        item = std::move(m_q.front());
        m_q.pop();
        return 0;
    }

    /*
    * @brief 阻塞获取队列大小
    * @return 队列大小
    */
    virtual int GetQueueSize() const override
    {
        {
            // 加锁
            std::unique_lock lock(m_mtx);
            return m_q.size();
        }
    }

    /// <summary>
    /// 关闭队列并通知所有等待方。
    /// </summary>
    void Close() override 
    {
        {
            std::unique_lock lock(m_mtx);
            m_closed = true;
        }
        m_cv.notify_all();
    }

private:
    /// <summary>底层 FIFO 容器。</summary>
    std::queue<T> m_q;
    /// <summary>队列关闭标记。</summary>
    bool m_closed{false};
    /// <summary>保护队列状态的互斥量。</summary>
    mutable std::mutex m_mtx;
    /// <summary>用于阻塞等待数据到达。</summary>
    std::condition_variable m_cv;
};

/*
* @brief 封装为阻塞的以ffmpeg提供的 AudioFifo 共享队列为基础的线程安全队列
*/
class AudioFifoQueue final : public IConcurrentQueue<AVFrame*>
{
public:
    typedef std::shared_ptr<AudioFifoQueue> ptr;

    AudioFifoQueue();

    virtual ~AudioFifoQueue();

    static IConcurrentQueue<AVFrame*>::ptr createNew(AVCodecContext* ctx, int count = 60);

    virtual bool Init_Queue() override { return true; };

    /**
    * @brief 阻塞并推入一个元素
    *
    * @param item 待入队元素
    */
    virtual void Push(AVFrame* item) override;

    /**
    * @brief 尝试无阻塞出队
    *
    * @return 有值表示成功出队；空值表示当前无数据
    */
    virtual std::optional<AVFrame*> TryPop() override;

    /**
    * @brief 阻塞等待并出队
    * 
    * @return 出队元素；关闭且无数据时返回默认值
    */
    virtual AVFrame* WaitAndPop() override;

    /**
    * @brief 阻塞等待并出队
    * @param T item 由调用者提供，内部从队列中取
    * @return 是否成功?
    */
    virtual int WaitAndPop(AVFrame* item) override;

    virtual int GetQueueSize() const override;

    /**
    * @brief 关闭队列并唤醒等待线程
    */
    virtual void Close() override;

    /*
    * @brief 初始化 m_aFifoBuf 分配合适的内存空间
    * @param aEncodeCtx 音频编码器上下文
    * @param count 分配队列空间时 m_nbSample 的数量，默认为 60
    */
    bool Init_Audio_Fifo(AVCodecContext* aEncodeCtx, int count = 60);

    /*
    * @brief 加锁获取队列内数据大小(采样点数)
    * @note 注意别死锁
    */
    const uint64_t get_audio_queue_size();

    /*
    * @brief 加锁获取队列内剩余空间大小(采样点数)
    * @note 注意别死锁
    */
    const uint64_t get_audio_queue_space();

    /*
    * @brief 清空队列
    */
    bool drain_audio_fifo_size();

private:

private:
    AVAudioFifo* m_aFifoBuf;
    std::mutex m_mtx;
    // 不空
    std::condition_variable m_cvNotEmpty;
    // 不满
    std::condition_variable m_cvNotFull;
    // 采样点数（编码器要求，1024 or 1152···）
    int m_nbSamples;
};


} // namespace streamer

#ifndef RENDERING_OPTIMIZATIONS_H
#define RENDERING_OPTIMIZATIONS_H

#include <QBrush>
#include <QFont>
#include <QFontMetrics>
#include <QHash>
#include <QMutex>
#include <QPainter>
#include <QPen>
#include <QStringView>
#include <QTextLayout>

namespace vte {

/**
 * @brief 字体度量缓存类，避免重复创建 QFontMetrics 对象
 */
class FontMetricsCache {
private:
  mutable QHash<QFont, QFontMetrics> m_cache;
  mutable QMutex m_mutex;

public:
  /**
   * @brief 获取字体度量，如果不存在则创建并缓存
   */
  const QFontMetrics &getMetrics(const QFont &font) const {
    QMutexLocker locker(&m_mutex);
    auto it = m_cache.find(font);
    if (it == m_cache.end()) {
      it = m_cache.insert(font, QFontMetrics(font));
    }
    return it.value();
  }

  /**
   * @brief 清理缓存
   */
  void clear() {
    QMutexLocker locker(&m_mutex);
    m_cache.clear();
  }

  /**
   * @brief 获取缓存大小
   */
  int size() const {
    QMutexLocker locker(&m_mutex);
    return m_cache.size();
  }
};

/**
 * @brief 文本布局缓存类，缓存 QTextLayout 对象
 */
class TextLayoutCache {
private:
  struct LayoutKey {
    QString text;
    QFont font;
    qreal width;

    bool operator==(const LayoutKey &other) const {
      return text == other.text && font == other.font && qAbs(width - other.width) < 0.001;
    }
  };

  // 为 LayoutKey 提供哈希函数
  friend uint qHash(const LayoutKey &key, uint seed = 0) {
    return qHash(key.text, seed) ^ qHash(key.font.toString(), seed) ^ qHash(key.width, seed);
  }

  mutable QHash<LayoutKey, QSharedPointer<QTextLayout>> m_cache;
  mutable QMutex m_mutex;
  static constexpr int MAX_CACHE_SIZE = 1000;

public:
  /**
   * @brief 获取文本布局，如果不存在则创建并缓存
   */
  QTextLayout &getLayout(const QString &text, const QFont &font, qreal width) {
    QMutexLocker locker(&m_mutex);

    // 限制缓存大小
    if (m_cache.size() > MAX_CACHE_SIZE) {
      m_cache.clear();
    }

    LayoutKey key{text, font, width};
    auto it = m_cache.find(key);
    if (it == m_cache.end()) {
      auto layout = QSharedPointer<QTextLayout>::create();
      layout->setText(text);
      layout->setFont(font);
      layout->setCacheEnabled(true);
      it = m_cache.insert(key, layout);
    }
    return *it.value();
  }

  /**
   * @brief 清理缓存
   */
  void clear() {
    QMutexLocker locker(&m_mutex);
    m_cache.clear();
  }
};

/**
 * @brief Painter 状态管理器，减少状态切换开销
 */
class PainterStateManager {
private:
  QPainter *m_painter;
  QFont m_originalFont;
  QPen m_originalPen;
  QFont m_currentFont;
  QPen m_currentPen;
  bool m_fontChanged = false;
  bool m_penChanged = false;

public:
  explicit PainterStateManager(QPainter *painter) : m_painter(painter) {
    m_originalFont = painter->font();
    m_originalPen = painter->pen();
    m_currentFont = m_originalFont;
    m_currentPen = m_originalPen;
  }

  /**
   * @brief 设置字体，只在必要时切换
   */
  void setFont(const QFont &font) {
    if (m_currentFont != font) {
      m_painter->setFont(font);
      m_currentFont = font;
      m_fontChanged = true;
    }
  }

  /**
   * @brief 设置画笔，只在必要时切换
   */
  void setPen(const QPen &pen) {
    if (m_currentPen != pen) {
      m_painter->setPen(pen);
      m_currentPen = pen;
      m_penChanged = true;
    }
  }

  /**
   * @brief 析构时恢复原始状态
   */
  ~PainterStateManager() {
    if (m_fontChanged) {
      m_painter->setFont(m_originalFont);
    }
    if (m_penChanged) {
      m_painter->setPen(m_originalPen);
    }
  }
};

/**
 * @brief 字符串视图优化类，避免不必要的字符串复制
 */
class StringViewOptimizer {
public:
  /**
   * @brief 使用 QStringView 计算文本宽度，避免字符串复制
   */
  static qreal calculateWidth(QStringView text, const QFontMetrics &fm) {
    qreal width = 0;
    for (int i = 0; i < text.length(); ++i) {
      width += fm.horizontalAdvance(text[i]);
    }
    return width;
  }

  /**
   * @brief 批量计算文本宽度
   */
  static void calculateWidths(const QVector<QStringView> &texts, const QFontMetrics &fm,
                              QVector<qreal> &widths) {
    widths.resize(texts.size());
    for (int i = 0; i < texts.size(); ++i) {
      widths[i] = calculateWidth(texts[i], fm);
    }
  }
};

/**
 * @brief 渲染性能统计类
 */
class RenderingProfiler {
private:
  mutable QElapsedTimer m_timer;
  mutable QHash<QString, qint64> m_timings;
  mutable QMutex m_mutex;

public:
  /**
   * @brief 开始计时
   */
  void startTiming(const QString &operation) const {
    QMutexLocker locker(&m_mutex);
    m_timer.start();
  }

  /**
   * @brief 结束计时并记录
   */
  void endTiming(const QString &operation) const {
    QMutexLocker locker(&m_mutex);
    qint64 elapsed = m_timer.elapsed();
    m_timings[operation] += elapsed;
  }

  /**
   * @brief 获取操作耗时
   */
  qint64 getTiming(const QString &operation) const {
    QMutexLocker locker(&m_mutex);
    return m_timings.value(operation, 0);
  }

  /**
   * @brief 清理统计
   */
  void clear() {
    QMutexLocker locker(&m_mutex);
    m_timings.clear();
  }

  /**
   * @brief 打印性能报告
   */
  void printReport() const {
    QMutexLocker locker(&m_mutex);
    qDebug() << "=== Rendering Performance Report ===";
    for (auto it = m_timings.begin(); it != m_timings.end(); ++it) {
      qDebug() << it.key() << ":" << it.value() << "ms";
    }
  }
};

} // namespace vte

#endif // RENDERING_OPTIMIZATIONS_H

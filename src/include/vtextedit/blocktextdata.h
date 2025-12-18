#ifndef BLOCKTEXTDATA_H
#define BLOCKTEXTDATA_H

#include <QAbstractTextDocumentLayout>
#include <QElapsedTimer>
#include <QFont>
#include <QFontMetrics>
#include <QHash>
#include <QMutex>
#include <QPainter>
#include <QPainterPath>
#include <QSharedPointer>
#include <QStringView>
#include <QTextBlock>
#include <QTextLayout>
#include <QVector>
#include <vtextedit/vtextedit_export.h>

#define UNCHANGED_ID 0
#define REMOVED_ID 1
#define REPLACED_ID 2
#define BLANKED_ID 3

// 性能优化相关常量
namespace RenderingConstants {
constexpr int MAX_FONT_CACHE_SIZE = 1000;
constexpr int MAX_LAYOUT_CACHE_SIZE = 500;
constexpr qreal ITALIC_WIDTH_FACTOR = 0.3;
constexpr int TAB_STOP_DISTANCE = 80;
constexpr int MAX_CACHE_MEMORY_MB = 50;
} // namespace RenderingConstants

// class QTextBlock;

namespace Sonnet {
class Speller;
class WordTokenizer;
} // namespace Sonnet

// 前向声明优化类
namespace vte {
class FontMetricsCache;
class TextLayoutCache;
class PainterStateManager;
class StringViewOptimizer;
class RenderingProfiler;
} // namespace vte

namespace vte {
struct RangeInfo {
  // format of visible part
  QTextCharFormat m_chf;

  // text of visible part
  QString m_text_changed;
  // width of visible part
  qreal m_width = 0;

  // tab or not
  bool m_is_tab = false;

  // position of text start in block
  int m_start = 0;
  int m_len = 0;

  int m_processId = UNCHANGED_ID;

  bool operator<(const RangeInfo &a) const {
    // 按start 升序排列
    if (m_start != a.m_start)
      return m_start < a.m_start;
    else // 起始位置相同，则按照长度降序排列
      return m_len < a.m_len;
  }
};

struct LineInfo {
  QTextLine m_tl;

  qreal m_width = 0;

  int m_start_new;
  int m_len_new;

  QVector<RangeInfo> m_ranges;

  LineInfo(QTextLine tl) {
    m_tl = tl;
    m_width = 0;
  }
};

/**
 * @brief 性能优化配置结构
 */
struct RenderingOptimizationConfig {
  bool enableFontCache = true;
  bool enableLayoutCache = true;
  bool enablePerformanceProfiling = false;
  bool enableBatchRendering = true;
  bool enableStringViewOptimization = true;
  int maxFontCacheSize = RenderingConstants::MAX_FONT_CACHE_SIZE;
  int maxLayoutCacheSize = RenderingConstants::MAX_LAYOUT_CACHE_SIZE;
};

class VTEXTEDIT_EXPORT BlockLinesData {
public:
  BlockLinesData() = default;
  // BlockLinesData(const QTextBlock* pblock,int cursorPosition);

  ~BlockLinesData();

  static QSharedPointer<BlockLinesData> get(const QTextBlock &p_block);

  void initBlockRanges(int cursorBlockNumber, const QTextBlock &pblock);
  void getBlockRanges(const QTextBlock &pblock);
  int getLineRanges(const QTextLine line, int start, const QTextBlock &pblock);

  // 性能优化方法声明 - 公有方法
  void getSuitableWidthOptimized(qreal distance, qreal &width, int &pos, RangeInfo &range,
                                 const QTextBlock &block);
  void rangeProcessWidthOptimized(const QTextBlock &block);
  void blockDrawOptimized(QPainter *painter, QPointF pos, QTextCharFormat selection_chf,
                          int firstLine, int lastLine, const QTextBlock &block);
  void drawOptimized(QPainter *painter, const QPointF &offset,
                     const QAbstractTextDocumentLayout::PaintContext &context,
                     const QVector<QTextLayout::FormatRange> &selections, QTextOption option,
                     QTextBlock &block);
  void drawSelectionsOptimized(QPainter *painter, const QPointF &position,
                               const QVector<QTextLayout::FormatRange> &selections,
                               const QRectF &clip, int firstLine, int lastLine,
                               const QTextBlock &block);

  // 缓存管理方法
  void clearCaches();

  // 获取行数信息
  int getLinesCount() const;

private:
  QVector<LineInfo> m_lines;

private:
  bool m_cursorBlock;
  QVector<RangeInfo> m_blockPreRanges;
  QVector<RangeInfo> m_blockRanges;

  // 性能优化相关成员
  QSharedPointer<FontMetricsCache> m_fontCache;
  QSharedPointer<TextLayoutCache> m_layoutCache;
  QSharedPointer<RenderingProfiler> m_profiler;
  RenderingOptimizationConfig m_optimizationConfig;

  // total lines width changed in block

  void setPenAndDrawBackground(QPainter *p, const QPen &defaultPen, const QTextCharFormat &chf,
                               const QRectF &r);
  // blockDraw 原始方法声明已删除，现在使用 blockDrawOptimized
  RangeInfo getRangesWidth(LineInfo *li, int start, int len, const QTextBlock &pblock);

  void processBlockText(const QTextBlock &block);
  void rangeAppend(RangeInfo &range, const QTextBlock &pblock);
  void rangeAppendWithFmt(RangeInfo &range);
  void rangeProcessFontStyle(const QTextBlock &pblock);
  void rangeRemoveFontStyle(int idx, QString sign_str, RangeInfo &range, const QTextBlock &block);
  // 原始方法声明已删除，现在使用优化版本
  static const QRectF clipIfValid(const QRectF &rect, const QRectF &clip);
  void addSelectedRegionsToPath(LineInfo *li, const QPointF &pos,
                                QTextLayout::FormatRange *selection, QPainterPath *region,
                                const QRectF &boundingRect, bool selectionStartInLine,
                                bool selectionEndInLine, const QTextBlock &pblock);

  bool posInWord(int &pos, int &tokenizer_len, Sonnet::WordTokenizer &wordTokenizer);
  bool atWordSeparator(int &pos, QString &text);

  // 缓存管理方法
  void initializeCaches();
  void enablePerformanceProfiling(bool enable);

  // 配置管理方法
  void setOptimizationConfig(const RenderingOptimizationConfig &config);
  RenderingOptimizationConfig getOptimizationConfig() const;

  // 性能统计方法
  void printPerformanceReport() const;
  void resetPerformanceCounters();
};

} // namespace vte
#endif // BLOCKTEXTATA_H

#include "rendering_optimizations.h"
#include <QDebug>
#include <QElapsedTimer>
#include <vtextedit/blocktextdata.h>

namespace vte {

/**
 * @brief 优化后的 getSuitableWidth 方法
 * 主要优化：
 * 1. 使用字体度量缓存
 * 2. 使用 QStringView 避免字符串复制
 * 3. 减少重复计算
 */
void BlockLinesData::getSuitableWidthOptimized(qreal distance, qreal &width, int &pos,
                                               RangeInfo &range, const QTextBlock &block) {
  // 检查缓存是否已初始化
  if (!m_fontCache) {
    // 尝试初始化缓存
    initializeCaches();
    if (!m_fontCache) {
      // 如果缓存初始化失败，直接返回，避免无限递归
      qWarning() << "Failed to initialize font cache in getSuitableWidthOptimized";
      return;
    }
  }

  // 使用缓存的字体度量
  const QFontMetrics &fm = m_fontCache->getMetrics(range.m_chf.font());

  // 使用 QStringView 避免字符串复制
  QStringView range_text = QStringView(block.text()).mid(range.m_start, range.m_len);

  // 预计算平均宽度，避免重复计算
  qreal average_width = range.m_width / range.m_len;
  int estimated_len = static_cast<int>(distance / average_width) + 1;

  // 优化：使用二分查找而不是线性搜索
  int left = 0, right = range.m_len;
  qreal best_width = 0;
  int best_pos = 0;

  while (left <= right) {
    int mid = (left + right) / 2;
    qreal current_width;

    if (range.m_processId == REPLACED_ID) {
      current_width =
          StringViewOptimizer::calculateWidth(QStringView(range.m_text_changed).mid(0, mid), fm);
    } else {
      current_width = StringViewOptimizer::calculateWidth(range_text.mid(0, mid), fm);
    }

    if (current_width <= distance) {
      best_width = current_width;
      best_pos = mid;
      left = mid + 1;
    } else {
      right = mid - 1;
    }
  }

  pos = best_pos;
  width = best_width;

  // 优化：使用词边界检测，避免在单词中间断行，同时避免标点符号出现在行首
  if (pos > 0 && pos < range.m_len) {
    QStringView text_to_check =
        (range.m_processId == REPLACED_ID) ? QStringView(range.m_text_changed) : range_text;

    QString text_str = text_to_check.toString();
    int original_pos = pos;

    // 断行策略：优先选择不会切割英文单词且不会导致标点符号在行首的断行点
    int best_break_pos = -1;

    // 向前查找断行点
    for (int i = pos; i >= 1; --i) {
      QChar currentChar = text_str.at(i); // 这是下一行的行首字符
      QChar prevChar = (i > 0) ? text_str.at(i - 1) : QChar();

      // 检查是否会切割英文单词
      bool wouldCutEnglishWord = false;
      if (i > 0) {
        // 如果前一个字符是英文单词字符，当前字符也是英文单词字符，则不能断行
        if (prevChar.isLetter() && prevChar.unicode() < 0x100 && currentChar.isLetter() &&
            currentChar.unicode() < 0x100) {
          wouldCutEnglishWord = true;
        }
      }

      // 检查下一行行首字符是否是空格或标点符号
      bool isPunctuationOrSpaceAtLineStart = false;
      if (currentChar.isSpace() || atWordSeparator(i, text_str)) {
        isPunctuationOrSpaceAtLineStart = true;
      }

      // 如果不会切割英文单词且下一行行首不是空格或标点符号，则这是一个好的断行点
      if (!wouldCutEnglishWord && !isPunctuationOrSpaceAtLineStart) {
        best_break_pos = i;
        break;
      }
    }

    // 选择最佳的断行点
    if (best_break_pos > 0) {
      // 使用不会切割英文单词且不会导致标点符号在行首的断行点
      pos = best_break_pos;
    } else {
      // 如果没有找到任何断行点，使用原始位置
      pos = original_pos;
    }
  }
}

/**
 * @brief 优化后的 rangeProcessWidth 方法
 * 主要优化：
 * 1. 使用字体度量缓存
 * 2. 批量处理相同字体的范围
 * 3. 减少重复的字符串操作
 */
void BlockLinesData::rangeProcessWidthOptimized(const QTextBlock &block) {
  // 检查缓存是否已初始化
  if (!m_fontCache) {
    // 尝试初始化缓存
    initializeCaches();
    if (!m_fontCache) {
      // 如果缓存初始化失败，直接返回，避免无限递归
      qWarning() << "Failed to initialize font cache in rangeProcessWidthOptimized";
      return;
    }
  }

  // 按字体分组，减少字体度量查询次数
  QHash<QFont, QVector<int>> fontGroups;
  //    qWarning() << "rangeProcessWidthOptimized: m_blockRanges.size"
  //               <<m_blockRanges.size()<<block.text();

  for (int i = 0; i < m_blockRanges.size(); ++i) {
    fontGroups[m_blockRanges[i].m_chf.font()].append(i);
  }
  // 批量处理相同字体的范围
  for (auto it = fontGroups.begin(); it != fontGroups.end(); ++it) {
    const QFont &font = it.key();
    const QVector<int> &indices = it.value();
    const QFontMetrics &fm = m_fontCache->getMetrics(font);

    // 预分配字符串缓冲区
    QString blockText = block.text();

    for (int idx : indices) {
      RangeInfo &range = m_blockRanges[idx];

      if (range.m_processId == REMOVED_ID) {
        range.m_width = 0;
      } else if (range.m_processId == REPLACED_ID) {
        range.m_width = StringViewOptimizer::calculateWidth(QStringView(range.m_text_changed), fm);
      } else {
        range.m_width = StringViewOptimizer::calculateWidth(
            QStringView(blockText).mid(range.m_start, range.m_len), fm);
      }

      // 斜体宽度调整
      if (range.m_chf.font().italic()) {
        range.m_width += 0.3 * range.m_width / range.m_len;
      }
    }
  }
}

/**
 * @brief 优化后的 blockDraw 方法
 * 主要优化：
 * 1. 使用 PainterStateManager 减少状态切换
 * 2. 按字体分组渲染
 * 3. 批量绘制相同字体的文本
 */
void BlockLinesData::blockDrawOptimized(QPainter *painter, QPointF pos,
                                        QTextCharFormat selection_chf, int firstLine, int lastLine,
                                        const QTextBlock &block) {
  // 检查缓存是否已初始化
  if (!m_fontCache) {
    // 尝试初始化缓存
    initializeCaches();
    if (!m_fontCache) {
      // 如果缓存初始化失败，直接返回，避免无限递归
      qWarning() << "Failed to initialize font cache in blockDrawOptimized";
      return;
    }
  }

  const QPen defaultPen = painter->pen();
  PainterStateManager stateManager(painter);

  // 按字体分组渲染，减少状态切换
  QHash<QFont, QVector<QPair<LineInfo *, RangeInfo *>>> fontGroups;

  for (int line = firstLine; line < lastLine; ++line) {
    // 检查行索引是否有效
    if (line >= m_lines.count()) {
      qWarning() << "blockDrawOptimized: line index out of bounds"
                 << "line=" << line << "m_linesCount=" << m_lines.count();
      continue;
    }

    LineInfo &li = m_lines[line];
    //        qWarning() << "blockDrawOptimized: processing line" << line
    //                   << "rangesCount=" << li.m_ranges.count()
    //                   << "lineNumber=" << li.m_tl.lineNumber();

    if (li.m_ranges.isEmpty()) {
      //            qWarning() << "blockDrawOptimized: skipping line" << line << "due to empty
      //            ranges";
      continue;
    }

    // 按字体分组
    int validRanges = 0;
    for (auto &range : li.m_ranges) {
      if (range.m_processId == REMOVED_ID || range.m_width == 0) {
        //                qWarning() << "blockDrawOptimized: skipping range"
        //                           << "processId=" << range.m_processId
        //                           << "width=" << range.m_width;
        continue;
      }
      fontGroups[range.m_chf.font()].append({&li, &range});
      validRanges++;
    }
  }

  // 按字体批量渲染
  for (auto it = fontGroups.begin(); it != fontGroups.end(); ++it) {
    const QFont &font = it.key();
    const auto &ranges = it.value();

    stateManager.setFont(font);

    // 批量绘制相同字体的文本
    for (const auto &pair : ranges) {
      LineInfo *li = pair.first;
      RangeInfo *range = pair.second;

      // 计算文本的累积位置，确保正确处理有序列表等缩进
      QPointF linePosition = pos + li->m_tl.position();
      qreal cumulativeWidth = 0;

      // 计算当前 range 之前所有 range 的累积宽度
      for (const auto &prevRange : li->m_ranges) {
        if (&prevRange == range)
          break;
        if (prevRange.m_processId != REMOVED_ID && prevRange.m_width > 0) {
          cumulativeWidth += prevRange.m_width;
        }
      }

      QPointF position = linePosition + QPointF(cumulativeWidth, 0);

      // 计算文本内容
      QString text;
      if (range->m_processId == REPLACED_ID) {
        text = range->m_text_changed;
      } else {
        text = block.text().mid(range->m_start, range->m_len);
      }

      // 设置画笔
      QTextCharFormat chf = range->m_chf;
      if (selection_chf.isValid()) {
        chf.merge(selection_chf);
      }

      QBrush fg = chf.foreground();
      if (fg.style() != Qt::NoBrush) {
        stateManager.setPen(QPen(fg, 0));
      } else {
        // 如果没有设置前景色，使用默认的文本颜色
        stateManager.setPen(defaultPen);
      }

      // 绘制文本
      if (range->m_processId != BLANKED_ID) {
        // 重新计算文本宽度，确保与实际字体度量一致
        QFontMetrics fm(font);
        qreal actualWidth = fm.horizontalAdvance(text);

        QRectF rect(position.x(), position.y(), actualWidth, li->m_tl.height());
        painter->drawText(rect, Qt::AlignVCenter, text);
      }
      //             else {
      //                qWarning() << "blockDrawOptimized: skipping BLANKED_ID range"
      //                           << "lineNumber=" << li->m_tl.lineNumber();
      //            }
    }
  }
}

/**
 * @brief 优化后的 draw 方法
 * 主要优化：
 * 1. 使用性能分析器监控关键操作
 * 2. 优化选择区域计算
 * 3. 减少重复的裁剪计算
 */
void BlockLinesData::drawOptimized(QPainter *painter, const QPointF &offset,
                                   const QAbstractTextDocumentLayout::PaintContext &context,
                                   const QVector<QTextLayout::FormatRange> &selections,
                                   QTextOption option, QTextBlock &block) {
  // 检查缓存是否已初始化
  if (!m_fontCache || !m_layoutCache || !m_profiler) {
    // 尝试初始化缓存
    initializeCaches();
    if (!m_fontCache || !m_layoutCache || !m_profiler) {
      // 如果缓存初始化失败，直接返回，避免无限递归
      qWarning() << "Failed to initialize caches in drawOptimized";
      return;
    }
  }

  // 检查行数据是否为空
  if (m_lines.isEmpty()) {
    // 如果行数据为空，直接返回，避免数据不一致
    // 数据应该在 textdocumentlayout.cpp 中通过条件判断来管理
    //        qWarning() << "m_lines is empty in drawOptimized, skipping rendering"
    //                   << "blockNumber=" << block.blockNumber()
    //                   << "textLength=" << block.text().length()
    //                   << "cursorBlockNumber=" << m_cursorBlock;
    return;
  }

  // 添加调试信息
  // qWarning() << "drawOptimized: rendering block"
  //             << "blockNumber=" << block.blockNumber()
  //             << "linesCount=" << m_lines.count()
  //             << "textLength=" << block.text().length();

  RenderingProfiler profiler;

  profiler.startTiming("draw_setup");

  // Qt6兼容性：设置默认文本颜色，确保文本显示为正常的黑色
  QPen oldPen = painter->pen();
  painter->setPen(context.palette.color(QPalette::Text));

  // Qt6高DPI兼容性：确保渲染精度
  painter->setRenderHint(QPainter::TextAntialiasing, true);
  painter->setRenderHint(QPainter::Antialiasing, true);

  QRectF clip = context.clip.isValid() ? context.clip : QRectF();
  QTextLayout *layout = block.layout();

  if (layout->lineCount() < 1)
    return;

  QPointF position = offset + layout->position();

  // 修复lastLine计算：正确处理坐标系统和边界条件
  int firstLine = 0;
  int lastLine = m_lines.count();

  if (clip.isValid()) {
    // 将裁剪区域转换为相对于layout的坐标系
    qreal clipTop = clip.y() - position.y();
    qreal clipBottom = clipTop + clip.height();

    for (int i = 0; i < m_lines.count(); ++i) {
      QTextLine sl = m_lines[i].m_tl;
      qreal lineTop = sl.position().y();
      qreal lineBottom = lineTop + sl.height();

      // 如果行完全在裁剪区域上方，跳过
      if (lineBottom <= clipTop) {
        firstLine = i + 1; // 修复：应该是下一行
        continue;
      }

      // 如果行完全在裁剪区域下方，设置lastLine并停止
      if (lineTop >= clipBottom) {
        lastLine = i; // 修复：不包含当前行
        break;
      }
    }
  }

  profiler.endTiming("draw_setup");

  // 优化选择区域处理
  profiler.startTiming("draw_selections");
  drawSelectionsOptimized(painter, position, selections, clip, firstLine, lastLine, block);
  profiler.endTiming("draw_selections");

  // 优化文本绘制
  profiler.startTiming("draw_text");
  blockDrawOptimized(painter, position, QTextCharFormat(), firstLine, lastLine, block);
  profiler.endTiming("draw_text");

  // 恢复原来的画笔
  painter->setPen(oldPen);
}

/**
 * @brief 优化后的选择区域绘制
 */
void BlockLinesData::drawSelectionsOptimized(QPainter *painter, const QPointF &position,
                                             const QVector<QTextLayout::FormatRange> &selections,
                                             const QRectF &clip, int firstLine, int lastLine,
                                             const QTextBlock &block) {
  QPainterPath excludedRegion;
  QPainterPath textDoneRegion;

  for (int i = 0; i < selections.size(); ++i) {
    QTextLayout::FormatRange selection = selections.at(i);
    QPainterPath region;
    region.setFillRule(Qt::WindingFill);

    for (int line = firstLine; line < lastLine; ++line) {
      // 检查行索引是否有效
      if (line >= m_lines.count() || line >= block.layout()->lineCount()) {
        continue;
      }
      LineInfo &li = m_lines[line];
      QTextLine tl = li.m_tl; // 使用 m_lines 中的 QTextLine，确保索引一致性

      QRectF lineRect(tl.naturalTextRect().x(), tl.naturalTextRect().y(), li.m_width,
                      tl.naturalTextRect().height());
      lineRect.translate(position);

      // 优化选择区域计算
      if (selection.format.boolProperty(QTextFormat::FullWidthSelection)) {
        if (selection.start != li.m_tl.textStart())
          continue;
        selection.start = li.m_start_new;
        selection.length = li.m_len_new;
      }

      bool isLastLineInBlock = (line == block.layout()->lineCount() - 1);
      int sl_length = li.m_len_new + (isLastLineInBlock ? 1 : 0);

      if (li.m_start_new > selection.start + selection.length ||
          li.m_start_new + sl_length <= selection.start) {
        continue;
      }

      const bool selectionStartInLine = li.m_start_new <= selection.start;
      const bool selectionEndInLine =
          selection.start + selection.length < li.m_start_new + li.m_len_new;

      if (tl.textLength() && (selectionStartInLine || selectionEndInLine)) {
        addSelectedRegionsToPath(&li, position, &selection, &region, clipIfValid(lineRect, clip),
                                 selectionStartInLine, selectionEndInLine, block);
      } else {
        region.addRect(clipIfValid(lineRect, clip));
      }
    }

    // 绘制选择区域
    const QPen oldPen = painter->pen();
    const QBrush oldBrush = painter->brush();

    painter->setPen(selection.format.penProperty(QTextFormat::OutlinePen));
    painter->setBrush(selection.format.brushProperty(QTextFormat::BackgroundBrush));
    painter->drawPath(region);

    painter->setPen(oldPen);
    painter->setBrush(oldBrush);

    excludedRegion += region;
  }

  // 绘制文本
  if (!excludedRegion.isEmpty()) {
    painter->save();
    QPainterPath path;
    QRectF br = block.layout()->boundingRect().translated(position);
    br.setRight(INT_MAX / 256);
    if (!clip.isNull()) {
      br = br.intersected(clip);
    }
    path.addRect(br);
    path -= excludedRegion;
    painter->setClipPath(path, Qt::IntersectClip);
  }

  blockDrawOptimized(painter, position, QTextCharFormat(), firstLine, lastLine, block);

  if (!excludedRegion.isEmpty()) {
    painter->restore();
  }
}

} // namespace vte

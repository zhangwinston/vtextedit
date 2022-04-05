#ifndef BLOCKTEXTDATA_H
#define BLOCKTEXTDATA_H

#include <QAbstractTextDocumentLayout>
#include <QVector>
#include <vtextedit/vtextedit_export.h>

#define UNCHANGED_ID 0
#define REMOVED_ID 1
#define REPLACED_ID 2
#define BLANKED_ID 3

class QTextBlock;

namespace vte
{
    struct RangeInfo
    {
        //format of visible part
        QTextCharFormat m_chf;

        //text of visible part
        QString m_text_changed;
        //width of visible part
        qreal m_width_changed=0;

        //tab or not
        bool m_is_tab=false;

        //position of text start in block
        int m_start=0;
        int m_len=0;

        int m_processId=UNCHANGED_ID;

        bool operator< (const RangeInfo &a)  const
            {
                //按start 升序排列
                if(m_start!=a.m_start)
                    return m_start<a.m_start;
                else //起始位置相同，则按照长度降序排列
                    return m_len<a.m_len;
            }
    };

    struct LineInfo
    {
        QTextLine m_tl;

        qreal m_width_visible=0;

        int m_start_new;
        int m_len_new;

        QVector<RangeInfo> m_ranges;

        LineInfo(QTextLine tl){
            m_tl=tl;
            m_width_visible=0;
        }
    };

    class VTEXTEDIT_EXPORT BlockLinesData
    {
    public:

        BlockLinesData() = default;
        //BlockLinesData(const QTextBlock* pblock,int cursorPosition);

        ~BlockLinesData();


        static QSharedPointer<BlockLinesData> get(const QTextBlock &p_block);


        void draw(QPainter *p, const QPointF &pos,const QAbstractTextDocumentLayout::PaintContext &p_context, const QVector<QTextLayout::FormatRange> &selections,QTextOption option, QTextBlock &pblock);
        void initBlockRanges(int cursorBlockNumber,const QTextBlock& pblock);
        void getBlockRanges(const QTextBlock& pblock);
        int getLineRanges(const QTextLine line,int start,const QTextBlock& pblock);

    private:

        bool m_cursorBlock;
        QVector<LineInfo> m_lines;
        QVector<RangeInfo> m_blockPreRanges;
        QVector<RangeInfo> m_blockRanges;

        //total lines width changed in block

        void setPenAndDrawBackground(QPainter *p, const QPen &defaultPen, const QTextCharFormat &chf, const QRectF &r);
        void blockDraw(QPainter *p_painter,QPointF pos,QTextCharFormat selection_chf, int firstLine, int lastLine,const QTextBlock& pblock);
        RangeInfo getRangesWidth(LineInfo* li, int start ,int len,const QTextBlock& pblock);

        void processBlockText(const QTextBlock& block);
        void rangeWithTabsAppend(RangeInfo* range, const QTextBlock& pblock);
        void rangeAppend(RangeInfo* range,const QTextBlock& pblock);
        static qreal getTextWidth(const QTextCharFormat chf, QString text);
        static const QRectF clipIfValid(const QRectF &rect, const QRectF &clip);
        void addSelectedRegionsToPath(LineInfo* li,const QPointF &pos, QTextLayout::FormatRange *selection,
                                      QPainterPath *region, const QRectF &boundingRect,
                                      bool selectionStartInLine, bool selectionEndInLine,const QTextBlock& pblock);
    };

}
#endif // BLOCKTEXTATA_H

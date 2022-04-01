#ifndef BLOCKTEXTDATA_H
#define BLOCKTEXTDATA_H

#include <QAbstractTextDocumentLayout>
#include <QVector>
#include <vtextedit/vtextedit_export.h>

class QTextBlock;

namespace vte
{
    struct RangeInfo
    {
        //format of visible part
        QTextCharFormat m_chf;
        //flag of range visbile content changed
        bool m_changed=false;
        bool m_visible=true;

        //text of visible part
        QString m_text_changed;
        //width of visible part
        qreal m_width_changed=0;

        //width of ranges before
        qreal m_width_changed_before=0;

        qreal m_width=0;
        //position of text start in block
        int m_start=0;
        int m_len=0;
    };

    struct LineInfo
    {
        QTextLine m_tl;

        qreal m_width_visible=0;
        qreal m_width=0;

        int m_start_new;
        int m_len_new;

        QVector<RangeInfo> m_ranges;

        LineInfo(QTextLine tl){
            m_tl=tl;
            m_width=0;
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
        void initBlockRanges(int cursorBlockNumber);
        int getLineRanges(const QTextLine line,int start,const QTextBlock& pblock);


    private:

        int m_cursorBlockNumber;
        QVector<LineInfo> m_lines;

        //total lines width changed in block

        void setPenAndDrawBackground(QPainter *p, const QPen &defaultPen, const QTextCharFormat &chf, const QRectF &r);
        void blockDraw(QPainter *p_painter,QPointF pos,QTextCharFormat selection_chf, int firstLine, int lastLine,const QTextBlock& pblock);
        void adjustLineWidth(LineInfo* li,const QTextBlock& pblock);
        RangeInfo getRangesWidth(LineInfo* li, int start ,int len,const QTextBlock& pblock);
        void processText(RangeInfo* range,QString text,const QTextCharFormat chf,LineInfo li,const QTextBlock& pblock);
        void rangeWithTabsAppend(RangeInfo* range, LineInfo* li,const QTextBlock& pblock);
        void rangeAppend(RangeInfo* range,LineInfo* line,const QTextBlock& pblock);
        static qreal getTextWidth(const QTextCharFormat chf, QString text);
        static const QRectF clipIfValid(const QRectF &rect, const QRectF &clip);
        void addSelectedRegionsToPath(LineInfo* li,const QPointF &pos, QTextLayout::FormatRange *selection,
                                      QPainterPath *region, const QRectF &boundingRect,
                                      bool selectionStartInLine, bool selectionEndInLine,const QTextBlock& pblock);
    };

}
#endif // BLOCKTEXTATA_H

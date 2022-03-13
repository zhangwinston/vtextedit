#ifndef BLOCKTEXTATA_H
#define BLOCKTEXTATA_H

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
        bool m_visible_changed=false;
        //text of visible part
        QString m_visible_text;
        //width of visible part
        qreal m_visible_width=0;
        //width of range (visible+hide)
        qreal m_total_width=0;
        //position of text start in block
        int m_start;
        //length of text in block
        int m_len;
        bool m_selection=false;
    };

    struct LineInfo
    {
        QTextLine m_tl;
        int m_tl_num=0;

        bool m_selectionStartInLine=false;
        bool m_selectionEndInLine=false;

        bool m_total_line=true; //true mean get range from line
        int m_part_begin;
        int m_part_len;

        qreal m_visible_width=0;
        qreal m_width=0;
        qreal m_line_visible_changed_width=0;
        QVector<RangeInfo> m_ranges;
        QVector<RangeInfo> m_select_ranges;
    };

    class VTEXTEDIT_EXPORT BlockTextData
    {
    public:

        BlockTextData() = default;
        BlockTextData(QTextBlock* pblock,int cursorPosition);

        ~BlockTextData();

        void draw(QPainter *p, const QPointF &pos,const QAbstractTextDocumentLayout::PaintContext &p_context, const QVector<QTextLayout::FormatRange> &selections,QTextOption option);

    private:
        QTextBlock* m_pblock;
        int m_cursorPosition;
        QVector<QTextLayout::FormatRange> m_pselections;
        QVector<LineInfo> m_lines;

        void getBlockLines();
        void setPenAndDrawBackground(QPainter *p, const QPen &defaultPen, const QTextCharFormat &chf, const QRectF &r);
        void lineDraw( LineInfo line, QPainter *p_painter,QPointF pos,QTextCharFormat selection_chf);
        void getLineRanges(LineInfo& lineInfo);
        RangeInfo getRangesWidth(QVector<RangeInfo> ranges);
        void rangeVisibleChange(RangeInfo* range,LineInfo line);
        static qreal getTextWidth(const QTextCharFormat chf, QString text);
        static const QRectF clipIfValid(const QRectF &rect, const QRectF &clip);
        void addSelectedRegionsToPath(LineInfo& li,const QPointF &pos, QTextLayout::FormatRange *selection,
                                      QPainterPath *region, const QRectF &boundingRect);

    };

}
#endif // BLOCKTEXTATA_H

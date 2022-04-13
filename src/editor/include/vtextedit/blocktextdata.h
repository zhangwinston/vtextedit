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

namespace Sonnet
{
    class Speller;
    class WordTokenizer;
}

namespace vte
{
    struct RangeInfo
    {
        //format of visible part
        QTextCharFormat m_chf;

        //text of visible part
        QString m_text_changed;
        //width of visible part
        qreal m_width=0;

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

        qreal m_width=0;

        int m_start_new;
        int m_len_new;

        QVector<RangeInfo> m_ranges;

        LineInfo(QTextLine tl){
            m_tl=tl;
            m_width=0;
        }
    };

    class VTEXTEDIT_EXPORT BlockLinesData
    {
    public:

        BlockLinesData()=default;
        //BlockLinesData(const QTextBlock* pblock,int cursorPosition);

        ~BlockLinesData();

        static QSharedPointer<BlockLinesData> get(const QTextBlock &p_block);

        void draw(QPainter *p, const QPointF &pos,const QAbstractTextDocumentLayout::PaintContext &p_context, const QVector<QTextLayout::FormatRange> &selections,QTextOption option, QTextBlock &pblock);
        void initBlockRanges(int cursorBlockNumber,const QTextBlock& pblock);
        void getBlockRanges(const QTextBlock& pblock);
        int getLineRanges(const QTextLine line,int start,const QTextBlock& pblock);

        void getSuitableWidth();
        
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
        void rangeAppend(RangeInfo& range, const QTextBlock& pblock);
        void rangeAppendWithFmt(RangeInfo& range);
        void rangeProcessFontStyle(const QTextBlock& pblock);
        void rangeRemoveFontStyle(int idx, QString sign_str,RangeInfo& range,const QTextBlock& block);
        void rangeProcessWidth(const QTextBlock& pblock);
        static const QRectF clipIfValid(const QRectF &rect, const QRectF &clip);
        void addSelectedRegionsToPath(LineInfo* li,const QPointF &pos, QTextLayout::FormatRange *selection,
                                      QPainterPath *region, const QRectF &boundingRect,
                                      bool selectionStartInLine, bool selectionEndInLine,const QTextBlock& pblock);

        void getSuitableWidth(qreal distance,qreal& width,int& pos,RangeInfo& range, const QTextBlock& block);
        bool posInWord(int& pos,int& tokenizer_len, Sonnet::WordTokenizer& wordTokenizer);
        bool atWordSeparator(int&pos, QString &text);
    };

}
#endif // BLOCKTEXTATA_H

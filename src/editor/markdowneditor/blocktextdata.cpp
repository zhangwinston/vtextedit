#include <vtextedit/blocktextata.h>

#include <QFontMetrics>
#include <QPainter>
#include <QRegularExpression>
#include <QPainterPath>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QDebug>

#define ObjectSelectionBrush (QTextFormat::ForegroundBrush + 1)
#define SuppressText 0x5012
#define SuppressBackground 0x513
#define QFIXED_MAX (INT_MAX/256)

using namespace vte;

BlockTextData::BlockTextData(QTextBlock* pblock,
                             int cursorPosition)
    : m_pblock(pblock),
      m_cursorPosition(cursorPosition)
{   
    getBlockLines();
}

BlockTextData::~BlockTextData(){

    if(!m_lines.isEmpty()){
        for(int i=0;i<m_lines.count();i++){
            LineInfo li=m_lines[i];
            li.m_ranges.clear();
            li.m_select_ranges.clear();
        }
        m_lines.clear();
    }
}

void BlockTextData::getBlockLines()
{
    for(int line=0; line< m_pblock->lineCount();line++){
        LineInfo li;
        li.m_tl=m_pblock->layout()->lineAt(line);
        li.m_tl_num=line;
        li.m_total_line=true;
        getLineRanges(li);

        m_lines.append(li);
    }
}

void BlockTextData::getLineRanges(LineInfo& li)
{
    QVector<RangeInfo> ranges;

    if(li.m_total_line && li.m_tl.textLength()<1) return;

    if(li.m_total_line==false && li.m_part_len < 1)return;

    QVector<QTextLayout::FormatRange> fmt = m_pblock->layout()->formats();

    //每行倒序绘制range，从尾部开始绘制
    int start=0;
    int end=0;
    if(li.m_total_line){
        start=li.m_tl.textStart();
        end=li.m_tl.textStart()+li.m_tl.textLength();
    }else{
        start=li.m_part_begin;
        end=li.m_part_begin+li.m_part_len;
    }

    int range_start=end;
    int range_end=end;

    bool range_get_fmt=false;
    //从最大序号的fmt开始绘制，有部份小序号的fmt貌似是中间态的，被包含在大序号的
    int idx=fmt.size()-1;

    do{
        range_end=range_start;
        range_start=start;
        range_get_fmt=false;

        for(; idx >= 0; idx--){
            auto fmt_start=fmt.at(idx).start;
            auto fmt_end=fmt_start+fmt.at(idx).length;

            if(range_end < fmt_start){ //fmt behind line
                continue;
            } else if (fmt_start < range_end && range_end <= fmt_end){//fmt cross line item end
                range_get_fmt=true;

                RangeInfo range;
                range.m_chf = fmt.at(idx).format;
                //fmt start before rang start, fmt contain range
                if(range_start< fmt_start ){
                    range_start=fmt_start;
                }
                range.m_start=range_start;
                range.m_len=range_end-range_start;

                rangeVisibleChange(&range,li);
                ranges.append(range);
                break;
            } else if(range_start < fmt_end && fmt_end < range_end){//fmt cross line item start ,or range contain fmt
                range_start=fmt_end;
                break;
            }else if(fmt_end <= range_start){//fmt ahead of line item
                break;
            }
        }

        if(range_get_fmt==false){ //no fmt fit to this line item
            RangeInfo range;
            range.m_chf=m_pblock->charFormat();
            range.m_start=range_start;
            range.m_len=range_end-range_start;

            rangeVisibleChange(&range,li);
            ranges.append(range);
        }
    }while(start < range_start);

    for(int i=ranges.count()-1; i>=0;i--){
        if(li.m_total_line){
            li.m_ranges.append(ranges.at(i));
        }else{
            li.m_select_ranges.append(ranges.at(i));
        }
    }
    return;
}

qreal BlockTextData::getTextWidth(const QTextCharFormat chf, QString text)
{
    QFont f = chf.font();
    QFontMetrics fm(f);
    return fm.horizontalAdvance(text);
}

void BlockTextData::rangeVisibleChange(RangeInfo* range, LineInfo li)
{
    QRegularExpression re;
    QRegularExpressionMatch match;
    QTextLine tl=li.m_tl;

    QString text=m_pblock->text().mid(range->m_start,range->m_len);
    range->m_total_width=getTextWidth(range->m_chf,text);

    //default range content is visible
    range->m_visible_changed=false;
    range->m_visible_width=range->m_total_width;

    //heading line hide "# "
    if(m_cursorPosition < m_pblock->position() || (m_cursorPosition >(m_pblock->position()+ tl.textLength()))){

        if(m_pblock->userState()==-1){ //not code block
            re.setPattern("^#+ +");
            match= re.match(text);
            if(match.hasMatch()){
                range->m_visible_changed=true;
                range->m_visible_text=text.replace(match.captured(),"");
                range->m_visible_width=getTextWidth(range->m_chf,text);
            }
        }

        //code inline hide "```"
        re.setPattern("^`{3}");
        match = re.match(text);
        if(match.hasMatch()) {
            range->m_visible_changed=true;
            range->m_visible_text=text.replace(match.captured(),"//");
            range->m_visible_width=getTextWidth(range->m_chf,text);
        }
    }

    if(m_cursorPosition < (m_pblock->position()+tl.textStart()) ||
            (m_cursorPosition >(m_pblock->position()+tl.textStart()+ tl.textLength()))){

        if(m_pblock->userState()==-1){ //not code block

            //code inline hide "`"
            re.setPattern("[^\\\\](`)");
            match = re.match(text);
            if(match.hasMatch()) {
                range->m_visible_changed=true;
                range->m_visible_text=text.replace(match.captured(1),"");
                range->m_visible_width=getTextWidth(range->m_chf,text);
            }

            //bold string hide "*, **, ***"
            re.setPattern("(\\*{1,3})[^ ]+");
            match = re.match(text);
            if(match.hasMatch()) {
                range->m_visible_changed=true;
                range->m_visible_text=text.replace(match.captured(1),"");
                range->m_visible_width=getTextWidth(range->m_chf,text);
            }

            //strik string hide "~~"
            re.setPattern("~~");
            match = re.match(text);
            if(match.hasMatch()) {
                range->m_visible_changed=true;
                range->m_visible_text=text.replace(match.captured(),"");
                range->m_visible_width=getTextWidth(range->m_chf,text);
            }
        }
    }
}

RangeInfo BlockTextData::getRangesWidth(QVector<RangeInfo> ranges)
{
    RangeInfo result;
    for(int i=0;i<ranges.count();i++){
        result.m_visible_width+=ranges.at(i).m_visible_width;
        result.m_total_width+=ranges.at(i).m_total_width;
    }
    return result;
}

void BlockTextData::setPenAndDrawBackground(QPainter *p, const QPen &defaultPen, const QTextCharFormat &chf, const QRectF &r)
{
    QBrush c = chf.foreground();
    if (c.style() == Qt::NoBrush) {
        p->setPen(defaultPen);
    }

    QBrush bg = chf.background();
    if (bg.style() != Qt::NoBrush && !chf.property(SuppressBackground).toBool())
        p->fillRect(r.toAlignedRect(), bg);
    if (c.style() != Qt::NoBrush) {
        p->setPen(QPen(c, 0));
    }
}

void BlockTextData::lineDraw( LineInfo li, QPainter *p_painter,QPointF pos,QTextCharFormat selection_chf)
{
    if (li.m_ranges.isEmpty()) {
        return;
    }

    QTextCharFormat chf=m_pblock->charFormat();

    if (selection_chf.isValid()) {
        chf.merge(selection_chf);
        QPen pen = p_painter->pen();
        setPenAndDrawBackground(p_painter, pen, chf, li.m_tl.rect());
    }

    QPointF position=pos+li.m_tl.position();

    qreal width=0;
    RangeInfo range;

    for (int i=0;i< li.m_ranges.count();i++) {
        range=li.m_ranges.at(i);

        position.rx()+=width;
        QTextCharFormat chf=range.m_chf;
        if (selection_chf.isValid()) {
            chf.merge(selection_chf);
        }

        QFont f = chf.font();
        f.resolve(QFont::AllPropertiesResolved);
        const QFont oldFont = p_painter->font();
        p_painter->setFont(f);

        const QPen oldPen = p_painter->pen();
        QBrush fg = chf.foreground();
        QPen pen;
        if (fg.style() != Qt::NoBrush) {
            pen.setBrush(fg);
            p_painter->setPen(pen);
        }

        QFontMetrics fm(f);

        QString text;
        if(range.m_visible_changed){
            text=range.m_visible_text;
        } else {
            text=m_pblock->text().mid(range.m_start,range.m_len);
        }

        width=fm.horizontalAdvance(text);

        QRectF rect(position.x(),position.y(),width,li.m_tl.height());

        p_painter->drawText(rect, Qt::AlignVCenter, text, 0);

        p_painter->setPen(oldPen);
        p_painter->setFont(oldFont);
    }
}

const QRectF BlockTextData::clipIfValid(const QRectF &rect, const QRectF &clip)
{
    return clip.isValid() ? (rect & clip) : rect;
}

void BlockTextData::addSelectedRegionsToPath(LineInfo& li,const QPointF &pos, QTextLayout::FormatRange *selection,
                                             QPainterPath *region, const QRectF &boundingRect)
{
    QPointF position=pos+li.m_tl.position();

    qreal range_off=0;
    qreal range_width=0;
    RangeInfo result;

    li.m_total_line=false;
    if(li.m_selectionStartInLine){ //get offset before range
        li.m_part_begin=li.m_tl.textStart();
        li.m_part_len= selection->start-li.m_tl.textStart();
        getLineRanges(li);
        result=getRangesWidth(li.m_select_ranges);
        range_off+=result.m_visible_width;
        li.m_line_visible_changed_width+=result.m_total_width-result.m_visible_width;
        li.m_select_ranges.clear();

        li.m_part_begin=selection->start;
    } else{
        li.m_part_begin=li.m_tl.textStart();
    }

    if(li.m_selectionEndInLine ){// get width to range end
        li.m_part_len=selection->start+selection->length - li.m_part_begin;
        getLineRanges(li);
        result=getRangesWidth(li.m_select_ranges);
        range_width+=result.m_visible_width;
        li.m_line_visible_changed_width+=result.m_total_width-result.m_visible_width;
        li.m_select_ranges.clear();

    }else{ // get width to line end

        li.m_part_len=li.m_tl.textStart()+li.m_tl.textLength() - li.m_part_begin;
        getLineRanges(li);
        result=getRangesWidth(li.m_select_ranges);
        range_width+=result.m_visible_width;
        li.m_line_visible_changed_width+=result.m_total_width-result.m_visible_width;
        li.m_select_ranges.clear();
    }

    qreal lineHeight = li.m_tl.height();

    if (range_width > 0){
        const QRectF rect = boundingRect & QRectF(position.x()+range_off, position.y(), range_width, lineHeight);
        region->addRect(rect.toAlignedRect());
    }
}

void BlockTextData::draw(QPainter *p, const QPointF &offset,const QAbstractTextDocumentLayout::PaintContext &p_context, const QVector<QTextLayout::FormatRange> &selections,QTextOption option)
{
    QRectF clip=QRectF();
    if(p_context.clip.isValid()){
        clip=p_context.clip;
    }
    QTextLayout *layout=m_pblock->layout();

    if(layout->lineCount()<1)
        return;

    QPointF position=offset+layout->position();

    qreal clipy = (INT_MIN/256);
    qreal clipe = (INT_MAX/256);

    int firstLine = 0;
    int lastLine = layout->lineCount();

    if (clip.isValid()) {
        clipy = clip.y() - position.y();
        clipe = clipy + clip.height();
    }
    for (int i = 0; i < layout->lineCount(); ++i) {
        QTextLine sl = layout->lineAt(i);
        if (sl.position().y() > clipe) {
            lastLine = i;
            break;
        }
        if ((sl.position().y()+ sl.height()) < clipy) {
            firstLine = i;
            continue;
        }
    }

    QPainterPath excludedRegion;
    QPainterPath textDoneRegion;

    QTextLine tl;

    for (int i = 0; i < selections.size(); ++i) {
        QTextLayout::FormatRange selection = selections.at(i);

        QPainterPath region;
        region.setFillRule(Qt::WindingFill);

        for (int line = firstLine; line < lastLine; ++line) {
            tl=layout->lineAt(line);
            LineInfo li=m_lines[line];

            QRectF lineRect(tl.naturalTextRect());
            lineRect.translate(position);

            //lineRect.adjust(0, 0, d->leadingSpaceWidth(sl).toReal(), 0);

            bool isLastLineInBlock = (line ==layout->lineCount()-1);
            int sl_length = tl.textLength() + (isLastLineInBlock? 1 : 0); // the infamous newline

            if (tl.textStart() > selection.start + selection.length ||tl.textStart() + sl_length <= selection.start){
                continue; // no actual intersection
            }

            const bool selectionStartInLine = tl.textStart()  <= selection.start;
            const bool selectionEndInLine = selection.start + selection.length < tl.textStart()  + sl_length;

            if (tl.textLength() && (selectionStartInLine || selectionEndInLine)) {
                li.m_selectionStartInLine=selectionStartInLine;
                li.m_selectionEndInLine=selectionEndInLine;
                addSelectedRegionsToPath(li, position, &selection, &region, clipIfValid(lineRect, clip));
            } else {
                region.addRect(clipIfValid(lineRect, clip));
            }

            if (selection.format.boolProperty(QTextFormat::FullWidthSelection)) {
                QRectF fullLineRect(tl.rect());
                fullLineRect.translate(position);
                fullLineRect.setRight(QFIXED_MAX);
                if (!selectionEndInLine){
                    region.addRect(clipIfValid(QRectF(lineRect.topRight(), fullLineRect.bottomRight()), clip));
                }
                if (!selectionStartInLine){
                    region.addRect(clipIfValid(QRectF(fullLineRect.topLeft(), lineRect.bottomLeft()), clip));
                }
            } else if (!selectionEndInLine
                       && isLastLineInBlock
                       &&!(option.flags() & QTextOption::ShowLineAndParagraphSeparators)) {
                region.addRect(clipIfValid(QRectF(lineRect.right()-li.m_line_visible_changed_width, lineRect.top(),
                                                  lineRect.height()/4, lineRect.height()), clip));
            }
        }
        {
            const QPen oldPen = p->pen();
            const QBrush oldBrush = p->brush();

            p->setPen(selection.format.penProperty(QTextFormat::OutlinePen));
            p->setBrush(selection.format.brushProperty(QTextFormat::BackgroundBrush));
            p->drawPath(region);

            p->setPen(oldPen);
            p->setBrush(oldBrush);
        }

        bool hasText = (selection.format.foreground().style() != Qt::NoBrush);
        bool hasBackground= (selection.format.background().style() != Qt::NoBrush);

        if (hasBackground) {
            selection.format.setProperty(ObjectSelectionBrush, selection.format.property(QTextFormat::BackgroundBrush));
            // don't just clear the property, set an empty brush that overrides a potential
            // background brush specified in the text
            selection.format.setProperty(QTextFormat::BackgroundBrush, QBrush());
            selection.format.clearProperty(QTextFormat::OutlinePen);
        }

        selection.format.setProperty(SuppressText, !hasText);

        if (hasText && !hasBackground && !(textDoneRegion & region).isEmpty())
            continue;

        p->save();
        p->setClipPath(region, Qt::IntersectClip);

        for (int line = firstLine; line < lastLine; ++line) {
              lineDraw(m_lines[line],p,position,selection.format);
        }
        p->restore();

        if (hasText) {
            textDoneRegion += region;
        } else {
            if (hasBackground)
                textDoneRegion -= region;
        }
        excludedRegion += region;
    }

    QPainterPath needsTextButNoBackground = excludedRegion - textDoneRegion;
    if (!needsTextButNoBackground.isEmpty()){
        p->save();
        p->setClipPath(needsTextButNoBackground, Qt::IntersectClip);
        QTextLayout::FormatRange selection;
        selection.start = 0;
        selection.length = INT_MAX;
        selection.format.setProperty(SuppressBackground, true);

        for (int line = firstLine; line < lastLine; ++line) {
              lineDraw(m_lines[line],p,position,selection.format);
        }
        p->restore();
    }

    if (!excludedRegion.isEmpty()) {
        p->save();
        QPainterPath path;
        QRectF br = layout->boundingRect().translated(position);
        br.setRight(QFIXED_MAX);
        if (!clip.isNull())
            br = br.intersected(clip);
        path.addRect(br);
        path -= excludedRegion;
        p->setClipPath(path, Qt::IntersectClip);
    }
    for (int line = firstLine; line< lastLine; ++line) {
          lineDraw(m_lines[line],p,position,QTextCharFormat());
    }
    if (!excludedRegion.isEmpty())
        p->restore();
}

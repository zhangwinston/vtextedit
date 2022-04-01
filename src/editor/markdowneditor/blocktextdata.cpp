#include <vtextedit/blocktextdata.h>
#include <vtextedit/textblockdata.h>
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

BlockLinesData::~BlockLinesData(){
    if(!m_lines.isEmpty()){
        for(int i=0;i<m_lines.count();i++){
            LineInfo li=m_lines[i];
            li.m_ranges.clear();
        }
        m_lines.clear();
    }
}

QSharedPointer<BlockLinesData> BlockLinesData::get(const QTextBlock &p_block)
{
    auto blockData = TextBlockData::get(p_block);
    auto data = blockData->getBlockLinesData();
    if (!data) {
        data.reset(new BlockLinesData());
        blockData->setBlockLinesData(data);
    }
    return data;
}

void BlockLinesData::initBlockRanges(int cursorBlockNumber)
{
    m_cursorBlockNumber=cursorBlockNumber;

    if(!m_lines.isEmpty()){
        for(int i=0;i<m_lines.count();i++){
            LineInfo li=m_lines[i];
            li.m_ranges.clear();
        }
        m_lines.clear();
    }
}

int BlockLinesData::getLineRanges(const QTextLine line,int start,const QTextBlock& block)
{
//    qWarning()<<"m_pblock text"<<block.text()<<"length"<<block.text().length();

        LineInfo li(line);

        li.m_start_new=start;
        if(li.m_start_new+li.m_tl.textLength() < block.text().length()){
            li.m_len_new=li.m_tl.textLength();
        }else{
            li.m_len_new=block.text().length()-li.m_start_new;
        }

        int range_start=li.m_start_new;
        int range_end=li.m_start_new;

        bool range_get_fmt=false;

        QVector<QTextLayout::FormatRange> fmt = block.layout()->formats();

        while(range_end < li.m_start_new+li.m_len_new)
        {
            range_start=range_end;
            range_end=li.m_start_new+li.m_len_new;
            range_get_fmt=false;

            for(int idx=0; idx <fmt.size(); idx++){
                auto fmt_start=fmt.at(idx).start;
                auto fmt_end=fmt_start+fmt.at(idx).length;

                if(fmt_end <= range_start ||range_end <= fmt_start){ //fmt before range
                    continue;
                } else if (fmt_start <= range_start && range_start < fmt_end){//fmt cross line item start, or fmt contain range
                    range_get_fmt=true;

                    RangeInfo range;

                    range.m_chf = fmt.at(idx).format;
                    range.m_start=range_start;

                    //fmt shorter than range, reset range to fmt end
                    if( fmt_end < range_end){
                        range_end=fmt_end;
                    }
                    range.m_len=range_end-range_start;

                    rangeWithTabsAppend(&range,&li,block);
                    break;
                } else if(range_start < fmt_start && fmt_start < range_end){//fmt start between range, or range contain fmt
                    range_end=fmt_start; //adjust range, look for matched fmt.
                    continue;
                }
            }

            if(range_get_fmt==false){ //no fmt fit to this line item
                RangeInfo range;
                range.m_chf=block.charFormat();
                range.m_start=range_start;
                range.m_len=range_end-range_start;

                rangeWithTabsAppend(&range,&li,block);
            }
        }

        //if cusor not in current block, hide some charactors need adjust content of line
        if(m_cursorBlockNumber!=block.blockNumber()){
            if(li.m_width_visible> li.m_tl.width()){
               adjustLineWidth(&li,block);
            }
        }

        m_lines.append(li);
        return (li.m_start_new+li.m_len_new);
}

void BlockLinesData::adjustLineWidth(LineInfo* li,const QTextBlock& block)
{
    qreal distance=li->m_width_visible - li->m_tl.naturalTextWidth();

    for(int i=li->m_ranges.count()-1;i>=0;i--){
        RangeInfo& range=li->m_ranges[i];
        if(range.m_width_changed < distance){
            li->m_ranges.remove(i);
            li->m_width_visible-=range.m_width_changed;
            li->m_width-=range.m_width;

            li->m_len_new-=range.m_len;
             continue;
        }else{
            //拆分部分range
            QString part_text;
            for(int i=range.m_len-1;i>=1 ;i--){
                RangeInfo ri;
                ri.m_chf =range.m_chf;
                ri.m_start=range.m_start;
                ri.m_len=i;

                QString text=block.text().mid(ri.m_start,ri.m_len);
                processText(&ri,text,ri.m_chf,*li,block);

                if(ri.m_changed){
                    part_text=ri.m_text_changed;
                } else {
                    part_text=block.text().mid(ri.m_start,ri.m_len);
                }

                if(range.m_width_changed-ri.m_width_changed > distance){
                    //add forward part info range, refresh preline ifon
                    if(ri.m_len<1)break;
                    li->m_width_visible-=range.m_width_changed;
                    li->m_width-=range.m_width;

                    distance-=range.m_width_changed;
                    li->m_len_new-=range.m_len;

                    range.m_start=ri.m_start;
                    range.m_len=ri.m_len;
                    range.m_changed=ri.m_changed;
                    range.m_text_changed=ri.m_text_changed;
                    range.m_width_changed=ri.m_width_changed;

                    li->m_width_visible+=range.m_width_changed;
                    li->m_width+=range.m_width;

                    distance+=range.m_width_changed;
                    li->m_len_new+=range.m_len;
                    break;
                }
            }
            break; //give up
        }
    }
}


qreal BlockLinesData::getTextWidth(const QTextCharFormat chf, QString text)
{
    QFont f = chf.font();
    QFontMetrics fm(f);
    return fm.horizontalAdvance(text);
}

void BlockLinesData::rangeWithTabsAppend(RangeInfo* range, LineInfo* li,const QTextBlock& block)
{
    QString range_text=block.text().mid(range->m_start,range->m_len);

    int index = 0;
    int start_pos=0;
    while ((index = range_text.indexOf("\t", index)) != -1) {
        if(index>start_pos)
        {
            RangeInfo ri;
            ri.m_chf=range->m_chf;
            ri.m_start=range->m_start+start_pos;
            ri.m_len=index-start_pos;
            rangeAppend(&ri,li,block);
        }
        RangeInfo ri;
        ri.m_chf=range->m_chf;
        ri.m_start=range->m_start+index;
        ri.m_len=1;

        rangeAppend(&ri,li,block);

        ++index;
        start_pos=index;
    }

    if(start_pos< range->m_len){
        RangeInfo ri;
        ri.m_chf=range->m_chf;
        ri.m_start=range->m_start+start_pos;
        ri.m_len=range->m_start+range->m_len-ri.m_start;
        rangeAppend(&ri,li,block);
    }
}

void BlockLinesData::rangeAppend(RangeInfo* range, LineInfo* li,const QTextBlock& block)
{
    QString text=block.text().mid(range->m_start,range->m_len);

    range->m_width_changed_before=li->m_width_visible;
    processText(range,text,range->m_chf,*li,block);

    //range has shorted len, so forward line length this len
    if(range->m_changed){
        int range_changed=range->m_len-range->m_text_changed.length();

        if(li->m_start_new +li->m_len_new + range_changed < block.text().length()){
            li->m_len_new+=range_changed;
        }else{
            li->m_len_new=(block.text().length()-li->m_start_new);
        }
    }

    li->m_width+=range->m_width;
    li->m_width_visible+=range->m_width_changed;
    li->m_ranges.append(*range);
}

void BlockLinesData::processText(RangeInfo* range,QString text,const QTextCharFormat chf, LineInfo li,const QTextBlock& block)
{
    QRegularExpression re;
    QRegularExpressionMatch match;

    range->m_len=text.length();
    range->m_changed=false;

    if(text=="\t"){
        qreal tab =block.layout()->textOption().tabStopDistance();
        if (tab <= 0)
            tab = 80; // default
        range->m_width=(floor(range->m_width_changed_before/tab)+1)*tab-range->m_width_changed_before;
        range->m_width_changed=range->m_width;
        return;
    }

    range->m_width=getTextWidth(chf,text);
    //default range content is visible
    range->m_changed=false;
    range->m_width_changed=range->m_width;


    //qWarning()<<"m_cursorBlockNumber"<<m_cursorBlockNumber<<"m_pblock text"<<block.text();
    if(m_cursorBlockNumber!=block.blockNumber()){

        //code block replace "```"
        re.setPattern("^`{3}\\S*");
        match = re.match(text);
        if(match.hasMatch()) {
            range->m_changed=true;
            //range->m_text_changed=text.replace(match.captured(),"_______");
            range->m_text_changed=text.replace(match.captured(),R"(▔▔▔▔)");
            range->m_width_changed=getTextWidth(chf,range->m_text_changed);
        }

        if(block.userState()==-1){ //not code block

            // heading line remove "# " in first line of block
            re.setPattern("^#+ +");
            match= re.match(text);
            if(match.hasMatch()){
                range->m_changed=true;
                range->m_text_changed=text.replace(match.captured(),"");
                range->m_width_changed=getTextWidth(chf,range->m_text_changed);
            }

            //code inline remove "`"
            re.setPattern("[^\\\\]{0,1}(`)");
            match = re.match(text);
            if(match.hasMatch()) {
                range->m_changed=true;
                range->m_text_changed=text.replace(match.captured(1),"");
                range->m_width_changed=getTextWidth(chf,range->m_text_changed);
            }

            //bold string remove "*, **, ***"
            re.setPattern("(\\*{1,3})[^ ]+");
            match = re.match(text);
            if(match.hasMatch()) {
                range->m_changed=true;
                range->m_text_changed=text.replace(match.captured(1),"");
                range->m_width_changed=getTextWidth(chf,range->m_text_changed);
            }

            //strik string remove "~~"
            re.setPattern("~~");
            match = re.match(text);
            if(match.hasMatch()) {
                range->m_changed=true;
                range->m_text_changed=text.replace(match.captured(),"");
                range->m_width_changed=getTextWidth(chf,range->m_text_changed);
            }

            //strik string hide math expression"\$(\S+)\$"
            re.setPattern("\\$(\\S+)\\$");
            re.setPatternOptions(QRegularExpression::DotMatchesEverythingOption | QRegularExpression::InvertedGreedinessOption);
            match = re.match(text);
            if(match.hasMatch()) {
                range->m_visible=false;
            }
        }
    }
}

void BlockLinesData::setPenAndDrawBackground(QPainter *p, const QPen &defaultPen, const QTextCharFormat &chf, const QRectF &r)
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

void BlockLinesData::blockDraw(QPainter *p_painter,QPointF pos,QTextCharFormat selection_chf, int firstLine, int lastLine,const QTextBlock& block)
{
    for (int line = firstLine; line< lastLine; ++line) {

        if(m_lines.count()<1){
            qWarning()<<"m_lines is empty!";
            return;
        }

        LineInfo li=m_lines[line];

        if (li.m_ranges.isEmpty()) {
            continue;
        }

        QTextCharFormat chf=block.charFormat();

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

            if(range.m_width==0)continue;

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

            QString text;
            if(range.m_changed){
                text=range.m_text_changed;
                width=range.m_width_changed;
            } else {
                text=block.text().mid(range.m_start,range.m_len);
                width=range.m_width;
            }

            QRectF rect(position.x(),position.y(),width,li.m_tl.height());

            if(range.m_visible){
                p_painter->drawText(rect, Qt::AlignVCenter, text, 0);
            }

            p_painter->setPen(oldPen);
            p_painter->setFont(oldFont);
        }
    }
}

const QRectF BlockLinesData::clipIfValid(const QRectF &rect, const QRectF &clip)
{
    return clip.isValid() ? (rect & clip) : rect;
}

RangeInfo BlockLinesData::getRangesWidth(LineInfo* li, int start_t ,int len_t,const QTextBlock& block)
{
    RangeInfo result,ri;
    QString text;

    result.m_width_changed=0;
    result.m_width=0;

    if(len_t==0){
        return result;
    }

    //total line
    if(start_t==li->m_start_new && len_t==li->m_len_new){
        result.m_width_changed=li->m_width_visible;
        result.m_width=li->m_width;
        return result;
    }

    for(int i=0;i<li->m_ranges.count();i++){
        RangeInfo ri=li->m_ranges[i];

        if(ri.m_start+ri.m_len <= start_t){ //text start after range
            continue;
        }

        if(start_t <ri.m_start+ri.m_len && ri.m_start+ri.m_len < start_t+len_t){ // range end between text, continue

            if(start_t<ri.m_start){//start_t between range, contain this full range
                text=block.text().mid(ri.m_start,ri.m_len);
            }else{
                text=block.text().mid(start_t,ri.m_start+ri.m_len-start_t);
            }
            processText(&ri,text,ri.m_chf,*li,block);
            result.m_width+=ri.m_width;
            result.m_width_changed+=ri.m_width_changed;
            continue;
        }

        if(start_t+len_t <=ri.m_start+ri.m_len && ri.m_start < start_t+len_t){ // text end between the range,  finished

            if(start_t<ri.m_start){ //end between range, start part of this range
                text=block.text().mid(ri.m_start,start_t+len_t-ri.m_start);
            }else{ //all in this range
                text=block.text().mid(start_t,len_t);
            }
            processText(&ri,text,ri.m_chf,*li,block);

            result.m_width+=ri.m_width;
            result.m_width_changed+=ri.m_width_changed;
            break;
        }

        if(start_t+len_t <= ri.m_start ){// text end before range,
            break;
        }
    }

    return result;
}


void BlockLinesData::addSelectedRegionsToPath(LineInfo* li,const QPointF &pos,
                                              QTextLayout::FormatRange *selection,
                                             QPainterPath *region, const QRectF &boundingRect,
                                              bool selectionStartInLine, bool selectionEndInLine,const QTextBlock& block)
{
    QPointF position=pos+li->m_tl.position();

    qreal selection_off=0, selection_width=0;
    RangeInfo result;
    qreal start=0, len=0;

    if(selectionStartInLine){ //get offset before range
        start=li->m_start_new;
        len=selection->start-li->m_start_new;
        result=getRangesWidth(li,start,len,block);
        selection_off+=result.m_width_changed;
        start=selection->start;
    } else{
        start=li->m_start_new;
    }
    if(selectionEndInLine ){// get width to range end
        len=selection->start+selection->length - start;
        result=getRangesWidth(li,start,len,block);
        selection_width+=result.m_width_changed;
    }else{ // get width to line end
        len=li->m_start_new+li->m_len_new- start;
        result=getRangesWidth(li,start,len,block);
        selection_width+=result.m_width_changed;
    }

    qreal lineHeight = li->m_tl.height();

    if (selection_width > 0){
        const QRectF rect = boundingRect & QRectF(position.x()+selection_off, position.y(), selection_width, lineHeight);
        region->addRect(rect.toAlignedRect());
    }
}

void BlockLinesData::draw(QPainter *p, const QPointF &offset,const QAbstractTextDocumentLayout::PaintContext &p_context,
                          const QVector<QTextLayout::FormatRange> &selections,QTextOption option,QTextBlock& block)
{
    QRectF clip=QRectF();
    if(p_context.clip.isValid()){
        clip=p_context.clip;
    }

    QTextLayout *layout=block.layout();

    if(layout->lineCount()<1)
        return;

    QPointF position=offset+layout->position();

    qreal clipy = (INT_MIN/256);
    qreal clipe = (INT_MAX/256);

    int firstLine = 0;
    //int lastLine = layout->lineCount();
    int lastLine =m_lines.count();

    if (clip.isValid()) {
        clipy = clip.y() - position.y();
        clipe = clipy + clip.height();
    }
    for (int i = 0; i < m_lines.count(); ++i) {
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

//        qWarning()<<"selection i"<<i<<block.text().mid(selection.start,selection.length)<<"start"<<selection.start;

        for (int line = firstLine; line < lastLine; ++line) {
            tl=layout->lineAt(line);
            LineInfo li=m_lines[line];

//            qWarning()<<"lineInfo i"<<line<<"start"<<li.m_start_new<<"len"<<li.m_len_new<<block.text().mid(li.m_start_new,li.m_len_new);

            QRectF lineRect(tl.naturalTextRect().x(),tl.naturalTextRect().y(),li.m_width_visible,tl.naturalTextRect().height());
            lineRect.translate(position);

            //lineRect.adjust(0, 0, d->leadingSpaceWidth(sl).toReal(), 0);
            if (selection.format.boolProperty(QTextFormat::FullWidthSelection)){
                if(selection.start !=li.m_tl.textStart()){
                    continue;
                }
                selection.start=li.m_start_new;
                selection.length=li.m_len_new;
             }

            bool isLastLineInBlock = (line ==layout->lineCount()-1);
            //int sl_length = tl.textLength() + (isLastLineInBlock? 1 : 0); // the infamous newline
            int sl_length = li.m_len_new + (isLastLineInBlock? 1 : 0); // the infamous newline

            //if (tl.textStart() > selection.start + selection.length || tl.textStart() + sl_length <= selection.start){
            if (li.m_start_new > selection.start + selection.length || li.m_start_new + sl_length <= selection.start){
                continue; // no actual intersection
            }

            const bool selectionStartInLine = li.m_start_new  <= selection.start;
            const bool selectionEndInLine = selection.start + selection.length < li.m_start_new  + li.m_len_new;

            if (tl.textLength() && (selectionStartInLine || selectionEndInLine)) {
                addSelectedRegionsToPath(&li, position, &selection, &region,
                                                    clipIfValid(lineRect, clip),
                                                    selectionStartInLine,
                                                    selectionEndInLine,block);
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
                region.addRect(clipIfValid(QRectF(lineRect.right(),
                                                  lineRect.top(),lineRect.height()/4,
                                                  lineRect.height()), clip));
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
        blockDraw(p,position,selection.format,firstLine,lastLine,block);
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
        blockDraw(p,position,selection.format,firstLine,lastLine,block);
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
    blockDraw(p,position,QTextCharFormat(),firstLine,lastLine,block);
    if (!excludedRegion.isEmpty())
        p->restore();
}

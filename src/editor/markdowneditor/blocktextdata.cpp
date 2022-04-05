#include <vtextedit/blocktextdata.h>
#include <vtextedit/textblockdata.h>
#include <QFontMetrics>
#include <QPainter>
#include <QRegularExpression>
#include <QPainterPath>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QDebug>
#include <QtAlgorithms>

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

void BlockLinesData::initBlockRanges(int cursorBlockNumber,const QTextBlock& block)
{
    m_cursorBlock=(cursorBlockNumber==block.blockNumber());

    if(!m_lines.isEmpty()){
        for(int i=0;i<m_lines.count();i++){
            LineInfo li=m_lines[i];
            li.m_ranges.clear();
        }
        m_lines.clear();
    }
    if(!m_blockPreRanges.isEmpty()){
        m_blockPreRanges.clear();
    }
    if(!m_blockRanges.isEmpty()){
        m_blockRanges.clear();
    }

    processBlockText(block);
}

void BlockLinesData::processBlockText(const QTextBlock& block)
{
    QRegularExpression re;
    QRegularExpressionMatchIterator matchi;
    QRegularExpressionMatch match;
    m_blockPreRanges.clear();

    if(!m_cursorBlock){

        //code block replace "```"
        re.setPattern("^(`{3}\\S*)$");
        match = re.match(block.text());
        if(match.hasMatch()) {
            RangeInfo ri;
            ri.m_processId=REPLACED_ID;
            ri.m_start = match.capturedStart(1);
            ri.m_len = match.capturedEnd(1)-ri.m_start;
            ri.m_text_changed=R"(▔▔▔▔)";
            m_blockPreRanges.push_back(ri);
        }

        if(block.userState()==-1){ //not code block

            // heading line remove "# " in first line of block
            re.setPattern("^(#+ +)");
            match = re.match(block.text());
            if(match.hasMatch()){
                RangeInfo ri;
                ri.m_processId=REMOVED_ID;
                ri.m_start = match.capturedStart(1);
                ri.m_len = match.capturedEnd(1)-ri.m_start;
                m_blockPreRanges.push_back(ri);
            }

            //hide url
            re.setPattern("\\[\\S+\\](\\(http[\\s\\S]+?\\))");
            matchi = re.globalMatch(block.text());
            while (matchi.hasNext()) {
                QRegularExpressionMatch match = matchi.next();

                RangeInfo ri;
                ri.m_processId=REMOVED_ID;
                ri.m_start = match.capturedStart(1);
                ri.m_len = match.capturedEnd(1)-ri.m_start;
                m_blockPreRanges.push_back(ri);
            }

            //code inline remove "`"
            re.setPattern("[^\\\\]{0,1}(`)");
            matchi = re.globalMatch(block.text());
            while (matchi.hasNext()) {
                QRegularExpressionMatch match = matchi.next();

                RangeInfo ri;
                ri.m_processId=REMOVED_ID;
                ri.m_start = match.capturedStart(1);
                ri.m_len = match.capturedEnd(1)-ri.m_start;
                m_blockPreRanges.push_back(ri);
            }

            //bold string remove "*, **, ***"
            re.setPattern("(\\*{1,3})[^ ]");
            matchi = re.globalMatch(block.text());
            while (matchi.hasNext()) {
                QRegularExpressionMatch match = matchi.next();

                RangeInfo ri;
                ri.m_processId=REMOVED_ID;
                ri.m_start = match.capturedStart(1);
                ri.m_len = match.capturedEnd(1)-ri.m_start;
                m_blockPreRanges.push_back(ri);
            }

            //strik string remove "~~"
            re.setPattern("~~");
            matchi = re.globalMatch(block.text());
            while (matchi.hasNext()) {
                QRegularExpressionMatch match = matchi.next();

                RangeInfo ri;
                ri.m_processId=REMOVED_ID;
                ri.m_start = match.capturedStart(1);
                ri.m_len = match.capturedEnd(1)-ri.m_start;
                m_blockPreRanges.push_back(ri);
            }

            //strik string hide math expression"\$(\S+)\$"
            re.setPattern("(\\$\\S+\\$)");
            re.setPatternOptions(QRegularExpression::DotMatchesEverythingOption | QRegularExpression::InvertedGreedinessOption);
            matchi = re.globalMatch(block.text());
            while (matchi.hasNext()) {
                QRegularExpressionMatch match = matchi.next();

                RangeInfo ri;
                ri.m_processId=BLANKED_ID;
                ri.m_start = match.capturedStart(1);
                ri.m_len = match.capturedEnd(1)-ri.m_start;
                m_blockPreRanges.push_back(ri);
            }
        }
    }
    std::sort(m_blockPreRanges.begin(),m_blockPreRanges.end());

//    for (int i = 0; i < m_blockPreRanges.size(); ++i) {
//        qWarning()<< "m_blockPreRanges"<<i<<"text"<<block.text().mid(m_blockPreRanges.at(i).m_start,m_blockPreRanges.at(i).m_len)<<"processId"<<m_blockPreRanges.at(i).m_processId <<"m_start"<< m_blockPreRanges.at(i).m_start<<"m_len"<<m_blockPreRanges.at(i).m_len;
//    }
}

void BlockLinesData::getBlockRanges(const QTextBlock& block)
{
//    qWarning()<<"m_pblock text"<<block.text()<<"length"<<block.text().length();

        int range_start=0;
        int range_end=0;

        bool range_get_fmt=false;

        QVector<QTextLayout::FormatRange> fmt = block.layout()->formats();

        while(range_end <block.text().length())
        {
            range_start=range_end;
            range_end=block.text().length();
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

                    rangeWithTabsAppend(&range,block);
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

                rangeWithTabsAppend(&range,block);
            }
        }

//        for (int i = 0; i < m_blockRanges.size(); ++i) {
//            qWarning()<< "m_blockRanges"<<i<<"processId"<<m_blockRanges.at(i).m_processId<<"m_start"<< m_blockRanges.at(i).m_start<<"m_len"<<m_blockRanges.at(i).m_len<<"text"<<block.text().mid(m_blockRanges.at(i).m_start,m_blockRanges.at(i).m_len);
//        }
}

int BlockLinesData::getLineRanges(const QTextLine line,int start,const QTextBlock& block)
{
        LineInfo li(line);

        li.m_start_new=0;
        li.m_len_new=0;

        qreal distance=0;

        for (int idx = 0; idx < m_blockRanges.size(); ++idx) {
        //for(auto &range:m_blockRanges){
            auto &range=m_blockRanges[idx];

            if(range.m_start<start)continue;

            if(li.m_ranges.count()==0 && range.m_start>start){
                qWarning()<< "GetLineRanges: Must be equal--- range.m_start";
                qWarning()<<"i="<<idx<<range.m_is_tab<<range.m_start<<range.m_len<<range.m_width_changed<<range.m_processId<<block.text().mid(range.m_start,range.m_len);
            }

            distance=li.m_tl.width()-li.m_width_visible;
//            qWarning()<< "li.m_tl.width"<<li.m_tl.width()<<"li.m_width_visible"<<li.m_width_visible;

            //set tab width with lineinfo
            if(range.m_is_tab==true){
                qreal tab =block.layout()->textOption().tabStopDistance();
                if (tab <= 0)
                    tab = 80; // default
                range.m_width_changed=(floor(li.m_width_visible/tab)+1)*tab-li.m_width_visible;
            }
            //add total range
            if(range.m_width_changed < distance){
                if(li.m_ranges.count()==0){
                    li.m_start_new=range.m_start;
                }
                li.m_len_new+=range.m_len;
                li.m_width_visible+=range.m_width_changed;
                li.m_ranges.append(range);
                continue;
            }
            //add part of range
            if(range.m_processId!=BLANKED_ID)
            {
                for(int j=range.m_len-1;j>=1 ;j--){
                    RangeInfo ri;
                    ri.m_chf =range.m_chf;
                    ri.m_start=range.m_start;
                    ri.m_len=j;
                    if(range.m_processId==REPLACED_ID){
                        ri.m_width_changed=getTextWidth(ri.m_chf,ri.m_text_changed.mid(0,ri.m_len));
                    }else{
                        ri.m_width_changed=getTextWidth(ri.m_chf,block.text().mid(ri.m_start,ri.m_len));
                    }
                    if(ri.m_width_changed < distance){
                        //add forward part of range
                        if(li.m_ranges.count()==0){
                            li.m_start_new=ri.m_start;
                        }
                        li.m_len_new+=ri.m_len;
                        li.m_width_visible+=ri.m_width_changed;
                        li.m_ranges.append(ri);

                        m_blockRanges.insert(idx,ri);
                        idx++;

                        auto &new_range=m_blockRanges[idx];

                        new_range.m_start=ri.m_start+ri.m_len;
                        new_range.m_len=new_range.m_len-ri.m_len;

                        if(new_range.m_processId==REPLACED_ID){
                            new_range.m_text_changed=ri.m_text_changed.mid(ri.m_len,ri.m_text_changed.length()-ri.m_len);
                            new_range.m_width_changed=getTextWidth(ri.m_chf,new_range.m_text_changed);
                        }else{
                            new_range.m_width_changed=getTextWidth(ri.m_chf,block.text().mid(new_range.m_start,new_range.m_len));
                        }
                        break;
                    }
                }
            }
            break; //give up
        }

        m_lines.append(li);

//        qWarning()<<"LineInfo number"<<li.m_tl.lineNumber();
//        for (int i=0;i< li.m_ranges.count();i++) {
//            RangeInfo range=li.m_ranges.at(i);
//            qWarning()<<"   ri"<<i<<range.m_is_tab<<range.m_start<<range.m_len<<range.m_width_changed<<range.m_processId<<block.text().mid(range.m_start,range.m_len);
//        }

//        qWarning()<<"m_blockRanges after get line"<<m_blockRanges.count();
//        for (int i = 0; i < m_blockRanges.count(); i++) {
//            RangeInfo pri=m_blockRanges.at(i);
//            qWarning()<<"   i="<<i<<pri.m_is_tab<<pri.m_start<<pri.m_len<<pri.m_width_changed<<pri.m_processId<<block.text().mid(pri.m_start,pri.m_len);
//        }

        return li.m_start_new+li.m_len_new;
}


qreal BlockLinesData::getTextWidth(const QTextCharFormat chf, QString text)
{
    QFont f = chf.font();
    QFontMetrics fm(f);
    return fm.horizontalAdvance(text);
}

void BlockLinesData::rangeWithTabsAppend(RangeInfo* range, const QTextBlock& block)
{
    if(range->m_len==0){ //blank block
        RangeInfo ri;
        ri.m_start=range->m_start;
        ri.m_len=range->m_len;
        ri.m_chf=range->m_chf;
        ri.m_processId=UNCHANGED_ID;
        ri.m_width_changed=0;
        m_blockRanges.append(ri);
        return;
    }

    //deal with tabs
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
            rangeAppend(&ri,block);
        }
        // append tab as a range
        RangeInfo ri;
        ri.m_chf=range->m_chf;
        ri.m_start=range->m_start+index;
        ri.m_len=1;
        ri.m_processId=UNCHANGED_ID;

        ri.m_is_tab=true;
        ri.m_width_changed=0;//adjust width while create lineinfo
        m_blockRanges.append(ri);

        ++index;
        start_pos=index;
    }

    if(start_pos< range->m_len){
        RangeInfo ri;
        ri.m_chf=range->m_chf;
        ri.m_start=range->m_start+start_pos;
        ri.m_len=range->m_start+range->m_len-ri.m_start;
        rangeAppend(&ri,block);
    }
}

void BlockLinesData::rangeAppend(RangeInfo* raw_range, const QTextBlock& block)
{
    QString text=block.text().mid(raw_range->m_start,raw_range->m_len);
    raw_range->m_width_changed=getTextWidth(raw_range->m_chf,text);

    raw_range->m_processId=UNCHANGED_ID;

    RangeInfo range=*raw_range;

    if(!m_cursorBlock){
        int range_end=range.m_start+range.m_len;

        for(int i=0;i<m_blockPreRanges.count();i++){
            RangeInfo pri=m_blockPreRanges.at(i);
            int pri_end=pri.m_start+pri.m_len;

            // pri move forward, three phase: pri before, pri.end between, pri start between, pri after

            //no cross, pri before range:   pri.start pri.end  range.start range.end
            if(pri_end <=range.m_start){
                continue;
            }

            //cross, pri.end between range: pri.start range.start pri.end range.end, or ange.start pri.start pri.end range.end, match part of range,continue
            if(range.m_start <pri_end && pri_end < range_end){

                    //add first part
                    if(range.m_start<pri.m_start){ // range.start [pri.start pri.end] range.end, continue,
                        RangeInfo part_range=range;
                        part_range.m_len=pri.m_start-range.m_start;
                        if(part_range.m_len>0){
                            part_range.m_width_changed=getTextWidth(part_range.m_chf,block.text().mid(part_range.m_start,part_range.m_len));
                            m_blockRanges.append(part_range);
                        }
                    }else{ //pri.start [range.start pri.end] range.end
                        if(pri.m_processId!=REPLACED_ID){
                            pri.m_len=pri_end-range.m_start;
                        }
                        pri.m_start=range.m_start;
                    }

                    //deal with second part, charFormat should set as range
                    pri.m_chf=range.m_chf;
                    pri.m_width_changed=getTextWidth(pri.m_chf,block.text().mid(pri.m_start,pri.m_len));

                    if(pri.m_processId==REMOVED_ID){
                        //clear m_width_changed
                        pri.m_width_changed=0;
                    }
                    if(pri.m_processId==REPLACED_ID){
                        //set m_width_changed= replaced text width
                        pri.m_width_changed=getTextWidth(pri.m_chf,pri.m_text_changed);
                    }
                    if(pri.m_processId==BLANKED_ID){
                        //do nothing
                    }

                    if(pri.m_len>0){
                        m_blockRanges.append(pri);
                    }

                    //deal with third part again
                    range.m_len=range_end-pri_end;
                    range.m_start=pri_end;

                continue;
            }

            //cross, range.end between pri,  finished: range.start pri.start range.end pri.end, or pri.start range.start range.end pri.end
            if(range_end <= pri_end && pri.m_start < range_end){

                //add first part
                if(range.m_start<pri.m_start){ //range.start [pri.start range.end] pri.end
                    RangeInfo part_range=range;
                    part_range.m_len=pri.m_start-range.m_start;
                    if(part_range.m_len>0){
                        part_range.m_width_changed=getTextWidth(part_range.m_chf,block.text().mid(part_range.m_start,part_range.m_len));
                        m_blockRanges.append(part_range);
                    }
                }else{ //pri.start [range.start range.end] pri.end
                    if(pri.m_processId!=REPLACED_ID){
                        pri.m_len=range.m_len;
                    }
                    pri.m_start=range.m_start;
                }

                //deal with second part, charFormat should set as range
                pri.m_chf=range.m_chf;
                pri.m_width_changed=getTextWidth(pri.m_chf,block.text().mid(pri.m_start,pri.m_len));

                if(pri.m_processId==REMOVED_ID){
                    //clear m_width_changed
                    pri.m_width_changed=0;
                }
                if(pri.m_processId==REPLACED_ID){
                    //set m_width_changed= replaced text width
                    pri.m_width_changed=getTextWidth(pri.m_chf,pri.m_text_changed);
                }
                if(pri.m_processId==BLANKED_ID){
                    //do nothing
                }

                if(pri.m_len>0){
                    m_blockRanges.append(pri);
                }

                //deal with third part, no more range part
                range.m_len=0;
                range.m_start=pri.m_start+pri.m_len;
                break;
            }

            //cross, pri.start after range, over
            if(range_end <= pri.m_start ){
                break;
            }
        }
    }

    if(range.m_len>0){
        text=block.text().mid(range.m_start,range.m_len);
        range.m_width_changed=getTextWidth(range.m_chf,text);
        m_blockRanges.append(range);
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

            if(range.m_processId==REMOVED_ID||range.m_width_changed==0)continue;

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
            if(range.m_processId==REPLACED_ID){
                text=range.m_text_changed;
            } else {
                text=block.text().mid(range.m_start,range.m_len);
            }
            width=range.m_width_changed;

            QRectF rect(position.x(),position.y(),width,li.m_tl.height());

            if(range.m_processId!=BLANKED_ID){
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

RangeInfo BlockLinesData::getRangesWidth(LineInfo* li, int sel_start ,int sel_len,const QTextBlock& block)
{
    RangeInfo result;

    result.m_width_changed=0;

    if(sel_len==0){
        return result;
    }

    //total line
    if(sel_start==li->m_start_new && sel_len==li->m_len_new){
        result.m_width_changed=li->m_width_visible;
        return result;
    }

    int sel_end=sel_start+sel_len;
    for(int i=0;i<li->m_ranges.count();i++){
        RangeInfo ri=li->m_ranges[i];
        int ri_end=ri.m_start+ri.m_len;

        if(ri_end <= sel_start){ //selection start after ri
            continue;
        }

        if(sel_start <ri_end && ri_end < sel_end){ // ri end between selection, add right hand part width to part of selection, continue
            //ri is tab,width is variable, so can't use getTextWidth here
            if(ri.m_is_tab){
                result.m_width_changed+=ri.m_width_changed;
            }else{
                if(sel_start<ri.m_start){// sel_start [ri.start ri_end] sel_end, continue
                    result.m_width_changed+=ri.m_width_changed;
                }else{ //ri.start [sel_start ri.end] sle_len
                    if(ri.m_processId==REPLACED_ID){
                        result.m_width_changed+=getTextWidth(ri.m_chf,ri.m_text_changed.mid(sel_start-ri.m_start,ri_end-sel_start));
                    }else if(ri.m_processId!=REMOVED_ID){
                        result.m_width_changed+=getTextWidth(ri.m_chf,block.text().mid(sel_start,ri_end-sel_start));;
                    }
                }
            }

            //find the right hand part of selection, sel_start and sel_len changed, sel_end hasn't changed
            sel_start=ri_end;
            sel_len=sel_end-sel_start;
            continue;
        }

        if(sel_end <=ri_end && ri.m_start < sel_end){ // selection end between the ri,  add left hand ri width to part of selection, finished
            //ri is tab,width is variable, so can't use getTextWidth here
            if(ri.m_is_tab){
                result.m_width_changed+=ri.m_width_changed;
                return result;
            }else{
                if(sel_start<ri.m_start){ //sel_start [ri.start sel_end] ri_end
                    if(ri.m_processId==REPLACED_ID){
                        result.m_width_changed+=getTextWidth(ri.m_chf,ri.m_text_changed.mid(0,sel_end-ri.m_start));
                    }else if(ri.m_processId!=REMOVED_ID){
                        result.m_width_changed+=getTextWidth(ri.m_chf,block.text().mid(ri.m_start,sel_end-ri.m_start));;
                    }
                }else{ // ri.start [sel_start sel_end] ri.end
                    if(ri.m_processId==REPLACED_ID){
                        result.m_width_changed+=getTextWidth(ri.m_chf,block.text().mid(sel_start-ri.m_start,sel_len));
                    }else if(ri.m_processId!=REMOVED_ID){
                        result.m_width_changed+=getTextWidth(ri.m_chf,block.text().mid(sel_start,sel_len));
                    }
                }
            }

            // if selection between removed ri, set mini width to indicated
            if(ri.m_processId==REMOVED_ID && result.m_width_changed==0){
                result.m_width_changed=0.5;
                return result;
            }

            break;
        }

        if(sel_end <= ri.m_start ){// selection end before range,
            break;
        }
    }

//    qWarning()<<"   selection"<<sel_start<<sel_end<<result.m_width_changed<<block.text().mid(sel_start,sel_len);

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

//            qWarning()<<"lineInfo i"<<line<<"start"<<li.m_start_new<<"len"<<li.m_len_new<<li.m_width_visible<<block.text().mid(li.m_start_new,li.m_len_new);

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

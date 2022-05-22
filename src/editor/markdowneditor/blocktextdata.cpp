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
#include <speller.h>
#include <tokenizer_p.h>
#include <QElapsedTimer>

#define ObjectSelectionBrush (QTextFormat::ForegroundBrush + 1)
#define SuppressText 0x5012
#define SuppressBackground 0x513
#define QFIXED_MAX (INT_MAX/256)

using namespace vte;

void print_range(QString tip, RangeInfo range, const QTextBlock& block)
{
     qWarning()<<tip<<range.m_start<<range.m_len<<range.m_width \
              <<range.m_processId<<range.m_is_tab<<block.text().mid(range.m_start,range.m_len);
}

void print_ranges(QString tip,QVector<RangeInfo> &ranges,const QTextBlock& block)
{
    qWarning()<<tip;
    for (int i = 0; i < ranges.size(); ++i) {
        print_range(QVariant(i).toString(),ranges.at(i),block);
    }
}

void print_chfRange(int index, QTextLayout::FormatRange &chf_range, const QTextBlock& block)
{
    qWarning()<<"fmt"<<index<<block.text().mid(chf_range.start,chf_range.length)<<"format"<<chf_range.format;
}

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

        QString block_text=block.text();

//        qWarning()<<"block number"<<block.blockNumber()<<"userstate"<<block.userState()<<block_text;

        if(block.userState()>=0){ //code block start, code block, code end
            //code block replace "```"
            re.setPattern("^(`{3}\\S*)$");
            match = re.match(block_text);
            if(match.hasMatch()) {
                RangeInfo ri;
                ri.m_processId=REPLACED_ID;
                ri.m_start = match.capturedStart(1);
                ri.m_len = match.capturedEnd(1)-ri.m_start;
                ri.m_text_changed="_";
                m_blockPreRanges.push_back(ri);
            }
        }

        if(block.userState()<0){ //not code block
            // heading line remove "# " in first line of block
            re.setPattern("^(#+ +)");
            match = re.match(block_text);
            if(match.hasMatch()){
                RangeInfo ri;
                ri.m_processId=REMOVED_ID;
                ri.m_start = match.capturedStart(1);
                ri.m_len = match.capturedEnd(1)-ri.m_start;
                m_blockPreRanges.push_back(ri);
            }

            // sperate lineith "***" in first line of block
            re.setPattern("^([*-]{3,})$");
            match = re.match(block_text);
            if(match.hasMatch()){
                RangeInfo ri;
                ri.m_processId=REPLACED_ID;
                ri.m_start = match.capturedStart(1);
                ri.m_len = match.capturedEnd(1)-ri.m_start;
                ri.m_text_changed="_";
                m_blockPreRanges.push_back(ri);
            }

            // ulist with "* " in line
//            re.setPattern("^(\\* ).+$");
//            match = re.match(block_text);
//            if(match.hasMatch()){
//                RangeInfo ri;
//                ri.m_processId=REPLACED_ID;
//                ri.m_start = match.capturedStart(1);
//                ri.m_len = match.capturedEnd(1)-ri.m_start;
//                ri.m_text_changed="•";
//                m_blockPreRanges.push_back(ri);
//                block_text=block_text.replace(ri.m_start,ri.m_len,blankStr.constData(),ri.m_len);
//            }

            //hide url/image file path
            //re.setPattern("(\\[.*?\\])(\\((?:[^\\(\\)]+|(?R))*+\\))");
            re.setPattern("(\\[.*?\\]\\()");
            matchi = re.globalMatch(block_text);
            while (matchi.hasNext()) {
                QRegularExpressionMatch match = matchi.next();
                int start=match.capturedEnd();
                int match_count = 1;
                int pos=start;
                // Traverse the input string
                for (; pos < block_text.length(); pos++){
                    if (block_text[pos] == '('){
                        match_count++;
                    }
                    else if (block_text[pos] == ')'){
                        if (match_count > 0)
                            match_count--;
                        if(match_count==0)
                            break;
                    }
                }
                if(match_count==0){
                    RangeInfo ri;
                    ri.m_processId=REMOVED_ID;
                    ri.m_start = start-1;
                    ri.m_len = pos-ri.m_start+1;
                    m_blockPreRanges.push_back(ri);
                }
            }

            //code inline remove "`"
            re.setPattern("[^\\\\]{0,1}(`)");
            matchi = re.globalMatch(block_text);
            while (matchi.hasNext()) {
                QRegularExpressionMatch match = matchi.next();

                RangeInfo ri;
                ri.m_processId=REMOVED_ID;
                ri.m_start = match.capturedStart(1);
                ri.m_len = match.capturedEnd(1)-ri.m_start;
                m_blockPreRanges.push_back(ri);
            }

            //hide math expression"\$(\S+)\$"
            re.setPattern("(\\$\\S+\\$)");
            re.setPatternOptions(QRegularExpression::DotMatchesEverythingOption | QRegularExpression::InvertedGreedinessOption);
            matchi = re.globalMatch(block_text);
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

//    print_ranges("block prepare ranges",m_blockPreRanges,block);
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
            auto chf_range=fmt.at(idx);
            auto fmt_start=chf_range.start;
            auto fmt_end=fmt_start+chf_range.length;

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

                rangeAppend(range,block);
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

            rangeAppend(range,block);
        }
    }
    std::sort(m_blockRanges.begin(),m_blockRanges.end());

    if(block.userState()<0){ //normal text
        rangeProcessFontStyle(block); //removed range font style sign, **/*/~~
    }

    rangeProcessWidth(block);

//    print_ranges("block ranges",m_blockRanges,block);
}

bool BlockLinesData::posInWord(int& pos,int& tokenizer_len,Sonnet::WordTokenizer& wordTokenizer)
{
    QStringRef w;
    bool inWord=false;
    while (wordTokenizer.hasNext()) {
        w = wordTokenizer.next();
        if(pos>(w.position()+w.length())) //after a word
            continue;

        if(w.position()< pos &&pos<(w.position()+w.length())){ //in a word
            tokenizer_len=w.position(); //for next loop
            pos=tokenizer_len;
            inWord=true;
            break;
        }

        if(pos<(w.position())){ //before a word, adjust to nearest word for next find loop
            tokenizer_len=w.position();
            break;
        }
    }
    return inWord;
}

void BlockLinesData::getSuitableWidth(qreal distance,qreal& width,int& pos,RangeInfo& range, const QTextBlock& block)
{
    QElapsedTimer time;
    time.start();

    QFontMetrics fm(range.m_chf.font());

    QString range_text=block.text().mid(range.m_start,range.m_len);
    qreal average_width=range.m_width/range.m_len;
    int estimated_len=distance/average_width+1;
    QTextLayout textLayout;
    textLayout.setText(range_text);
    textLayout.setFont(range.m_chf.font());
    textLayout.setCacheEnabled(true);


//    qWarning()<<"range text"<<range_text;

    //get the right length
    qreal gap=distance-width;
    while(gap>0 && estimated_len>0){
        pos+=estimated_len;
        if(pos>range.m_len){
            pos=range.m_len;
            break;
        }else if(pos<=0){
            break;
        }

        if(range.m_processId==REPLACED_ID){
            width=fm.horizontalAdvance(range.m_text_changed.mid(0,pos));
        }else{
            width=fm.horizontalAdvance(range_text.mid(0,pos));
        }
        gap=distance-width;
        estimated_len=gap/average_width+1;
    }
//    qWarning()<<"estimated_len"<<estimated_len<<"gap"<<gap<<"distance"<<distance<<"width"<<width;

    int tokenizer_len=range.m_len;
    Sonnet::WordTokenizer wordTokenizer;
    wordTokenizer.setBuffer(range_text);

    //get the right break position, avoid word or separator
    for(int i=pos;i>=1;i--){

        while(i>1 && atWordSeparator(i,range_text)){
            i--;
        }
        wordTokenizer.setBuffer(range_text.mid(0,tokenizer_len));
        posInWord(i,tokenizer_len, wordTokenizer);

        pos=i;
        if(i<=0){
            break; //at start of range, add nothing
        }
        if(range.m_processId==REPLACED_ID){
            width=fm.horizontalAdvance(range.m_text_changed.mid(0,i));
        }else{
            width=fm.horizontalAdvance(range_text.mid(0,i));
        }
        if(width <= distance){
            break;
        }
    }
}

int BlockLinesData::getLineRanges(const QTextLine line,int start,const QTextBlock& block)
{
    QElapsedTimer time;
    time.start();

    LineInfo li(line);

    li.m_start_new=0;
    li.m_len_new=0;

    qreal distance=0;

    for (int idx = 0; idx < m_blockRanges.size(); ++idx) {

        auto &range=m_blockRanges[idx];

//        print_range("satrt "+QVariant(start).toString()+" "+QVariant(idx).toString(),m_blockRanges.at(idx),block);

        if(range.m_start<start)continue;

        if(li.m_ranges.count()==0 && (range.m_start>start)){
            qWarning()<< "getLineRanges: li.m_ranges.count()==0 && start!=range.m_start" <<li.m_ranges.count()<<"start"<<range.m_start<<"i"<<idx;
            print_ranges("block ranges",m_blockRanges,block);
        }

        distance=li.m_tl.width()-li.m_width;

        if(range.m_processId==REPLACED_ID && range.m_text_changed=="_"){
            int count=distance/range.m_width;
            range.m_text_changed=range.m_text_changed.fill('_',count);
            range.m_width=count*range.m_width;
            if(li.m_ranges.count()==0){
                li.m_start_new=range.m_start;
            }
            li.m_len_new+=range.m_len;
            li.m_width+=range.m_width;
            li.m_ranges.append(range);
            break;
        }
        //set tab width with lineinfo
        if(range.m_is_tab==true){
            qreal tab =block.layout()->textOption().tabStopDistance();
            if (tab <= 0)
                tab = 80; // default
            range.m_width=(floor(li.m_width/tab)+1)*tab-li.m_width;
        }
        //add total range
        if(range.m_width <= distance){
            if(li.m_ranges.count()==0){
                li.m_start_new=range.m_start;
            }
            li.m_len_new+=range.m_len;
            li.m_width+=range.m_width;
            li.m_ranges.append(range);
            continue;
        }
        //add part of range
        if(range.m_processId!=BLANKED_ID)
        {
            QFontMetrics fm(range.m_chf.font());
            int pos=0;
            qreal width=0;

            getSuitableWidth(distance,width,pos,range,block);

            if(width <= distance){
                RangeInfo ri;
                ri.m_chf =range.m_chf;
                ri.m_start=range.m_start;
                ri.m_len=pos;
                ri.m_width=width;

                //add forward part of range
                if(li.m_ranges.count()==0){
                    li.m_start_new=ri.m_start;
                }
                li.m_len_new+=ri.m_len;
                li.m_width+=ri.m_width;
                li.m_ranges.append(ri);

                m_blockRanges.insert(idx,ri);
                idx++;

                auto &new_range=m_blockRanges[idx];

                new_range.m_start=ri.m_start+ri.m_len;
                new_range.m_len=new_range.m_len-ri.m_len;

                if(new_range.m_processId==REPLACED_ID){
                    new_range.m_text_changed=range.m_text_changed.mid(ri.m_len,ri.m_text_changed.length()-ri.m_len);
                    new_range.m_width=fm.horizontalAdvance(new_range.m_text_changed);
                }else{
                    new_range.m_width=fm.horizontalAdvance(block.text().mid(range.m_start+ri.m_len,new_range.m_len));
                }
            }
//            if(width > distance){
//                qWarning()<<"ERROR! width"<<width<<"distance"<<distance<<"pos"<<pos<<"idx"<<idx<<block.text().mid(range.m_start,range.m_len);
//                print_ranges("block ranges",m_blockRanges,block);
//                print_ranges(QStringLiteral("line number %1").arg(li.m_tl.lineNumber()),li.m_ranges,block);
//            }
            //the range not suitable for this line,so let's go
            break;
        }
    }
    m_lines.append(li);

    //    print_ranges(QStringLiteral("line number %1").arg(li.m_tl.lineNumber()),li.m_ranges,block);

    return li.m_start_new+li.m_len_new;
}

void BlockLinesData::rangeAppend(RangeInfo& range, const QTextBlock& block)
{
    if(range.m_len==0){ //blank block
        RangeInfo ri;
        ri.m_start=range.m_start;
        ri.m_len=range.m_len;
        ri.m_chf=range.m_chf;
        ri.m_processId=UNCHANGED_ID;
        ri.m_width=0;
        m_blockRanges.append(ri);
        return;
    }
    //deal with tabs
    QString range_text=block.text().mid(range.m_start,range.m_len);

    int index = 0;
    int start_pos=0;
    while ((index = range_text.indexOf("\t", index)) != -1) {
        if(index>start_pos)
        {
            RangeInfo ri;
            ri.m_chf=range.m_chf;
            ri.m_start=range.m_start+start_pos;
            ri.m_len=index-start_pos;
            rangeAppendWithFmt(ri);
        }
        // append tab as a range
        RangeInfo ri;
        ri.m_chf=range.m_chf;
        ri.m_start=range.m_start+index;
        ri.m_len=1;
        ri.m_processId=UNCHANGED_ID;

        ri.m_is_tab=true;
        ri.m_width=0;//adjust width while create lineinfo
        m_blockRanges.append(ri);

        ++index;
        start_pos=index;
    }

    if(start_pos< range.m_len){
        RangeInfo ri;
        ri.m_chf=range.m_chf;
        ri.m_start=range.m_start+start_pos;
        ri.m_len=range.m_start+range.m_len-ri.m_start;
        rangeAppendWithFmt(ri);
    }
}

void BlockLinesData::rangeAppendWithFmt(RangeInfo& range)
{
    range.m_processId=UNCHANGED_ID;

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
        m_blockRanges.append(range);
    }
}


void BlockLinesData::rangeProcessFontStyle(const QTextBlock& block)
{
    for (int i = 0; i < m_blockRanges.size(); ++i) {
        RangeInfo range=m_blockRanges[i];

//        print_range("rangeProcessFontStyle",range,block);

        if(range.m_processId==REMOVED_ID)continue;

        if(range.m_chf.font().bold()){
            rangeRemoveFontStyle(i,"**",range,block);
        }
        range=m_blockRanges[i]; //since above call maybe insert range, so assign range value again
        if(range.m_chf.font().italic()){
            rangeRemoveFontStyle(i,"*",range,block);
        }
        range=m_blockRanges[i];//since above call maybe insert range, so assign range value again
        if(range.m_chf.font().strikeOut()){
            rangeRemoveFontStyle(i,"~~",range,block);
        }
    }
}

void BlockLinesData::rangeRemoveFontStyle(int idx,QString sign_str,RangeInfo& range,const QTextBlock& block)
{
    QString text=block.text().mid(range.m_start,range.m_len);

    QVector<RangeInfo> tmp_ranges;

    //less than sign length, no need process, but head sign or tail sign maybe in two range
    if(text.length()<sign_str.length())return;

//    qWarning()<<"rangeRemoveFontStyle"<<text;
    RangeInfo ri_h;
    ri_h.m_chf=range.m_chf;
    ri_h.m_processId=REMOVED_ID;
    ri_h.m_start=range.m_start;
    ri_h.m_len=0;
    if(text.startsWith(sign_str)){
        ri_h.m_len=sign_str.length();
    }
    if(ri_h.m_len>0){ //save to tmp ranges, insert later
        tmp_ranges.append(ri_h);
    }
//    print_range("ri_h",ri_h,block);

    //insert tail sign at last
    RangeInfo ri_t;
    ri_t.m_chf=range.m_chf;
    ri_t.m_processId=REMOVED_ID;
    ri_t.m_len=0;
    if(text.length()-ri_h.m_len>sign_str.length()){
        if(text.endsWith(sign_str)){
            ri_t.m_len=sign_str.length();
        }
        ri_t.m_start=range.m_start+range.m_len-ri_t.m_len;
    }
//    print_range("ri_t",ri_t,block);

    //range will be changed,so save at first, caution: don't use range after insert
    int old_len=range.m_len;
    int old_start=range.m_start;

    //since two signs exist
    QRegularExpression re;
    re.setPattern("[^*]+");
    int start_pos=0;
    if(ri_h.m_len>0 && ri_t.m_len>0 && text.contains(re)){
        //sometime two bold/italic/strikeout str connect together
        text=text.mid(ri_h.m_len,range.m_len-2*sign_str.length());
        if(text.length()>2*sign_str.length()&&(!text.startsWith(sign_str)&&(!text.endsWith(sign_str)))){
            int index = 0;
            while ((index = text.indexOf(sign_str+sign_str, start_pos)) != -1) {
                if(index>=start_pos)
                {
                    RangeInfo ri;
                    ri.m_chf=range.m_chf;
                    ri.m_start=range.m_start+ri_h.m_len+start_pos;
                    ri.m_processId=range.m_processId;
                    ri.m_len=index-start_pos;
                    if(ri.m_len>0){
                        tmp_ranges.append(ri);
                    }
                    start_pos+=ri.m_len;

                    // remove two sign_str
                    ri.m_start=range.m_start+ri_h.m_len+index;
                    ri.m_len=2*sign_str.length();
                    ri.m_processId=REMOVED_ID;
                    tmp_ranges.append(ri);
                    start_pos+=ri.m_len;
                    continue;
                }
            }
        }

        std::sort(tmp_ranges.begin(),tmp_ranges.end());
//        print_ranges("tmp_ranges",tmp_ranges,block);

        RangeInfo ri;
        foreach (ri, tmp_ranges){
            m_blockRanges.insert(idx,ri);
            idx++;
        }
    }

    RangeInfo &modify_range=m_blockRanges[idx];
    modify_range.m_start=old_start+ri_h.m_len+start_pos;
    modify_range.m_len=old_len-ri_h.m_len-ri_t.m_len-start_pos;
    //ex: only part of bold sign, remove it only
    if(modify_range.m_len<=0){
        modify_range.m_processId=REMOVED_ID;
        modify_range.m_start=old_start;
        modify_range.m_len=old_len;
    }

    if(ri_t.m_len>0){
        idx++;
        m_blockRanges.insert(idx,ri_t);
    }
//    print_ranges("m_blockRanges",m_blockRanges,block);
}


void BlockLinesData::rangeProcessWidth(const QTextBlock& block)
{
    QElapsedTimer time;
    time.start();

    for (int i = 0; i < m_blockRanges.size(); ++i) {
        RangeInfo &range=m_blockRanges[i];
        QFontMetrics fm(range.m_chf.font());

        range.m_width=fm.horizontalAdvance(block.text().mid(range.m_start,range.m_len));
        if(range.m_processId==REMOVED_ID){
            //clear m_width
            range.m_width=0;
        }
        if(range.m_processId==REPLACED_ID){
            //set m_width= replaced text width
            range.m_width=fm.horizontalAdvance(range.m_text_changed);
        }
        if(range.m_processId==BLANKED_ID){
            //do nothing
        }
        if(range.m_chf.font().italic()){
            //set m_width= text width + 0.3width of char
            range.m_width+=0.3*range.m_width/range.m_len;
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

            if(range.m_processId==REMOVED_ID||range.m_width==0)continue;

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
            width=range.m_width;

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

    result.m_width=0;

    if(sel_len==0){
        return result;
    }

    //total line
    if(sel_start==li->m_start_new && sel_len==li->m_len_new){
        result.m_width=li->m_width;
        return result;
    }

    int sel_end=sel_start+sel_len;
    for(int i=0;i<li->m_ranges.count();i++){
        RangeInfo ri=li->m_ranges[i];
        int ri_end=ri.m_start+ri.m_len;

        if(ri_end <= sel_start){ //selection start after ri
            continue;
        }

        QFontMetrics fm(ri.m_chf.font());

        if(sel_start <ri_end && ri_end < sel_end){ // ri end between selection, add right hand part width to part of selection, continue
            //ri is tab,width is variable, so can't use getTextWidth here
            if(ri.m_is_tab){
                result.m_width+=ri.m_width;
            }else{
                if(sel_start<ri.m_start){// sel_start [ri.start ri_end] sel_end, continue
                    result.m_width+=ri.m_width;
                }else{ //ri.start [sel_start ri.end] sle_len
                    if(ri.m_processId==REPLACED_ID){
                        result.m_width+=fm.horizontalAdvance(ri.m_text_changed.mid(sel_start-ri.m_start,ri_end-sel_start));
                    }else if(ri.m_processId!=REMOVED_ID){
                        result.m_width+=fm.horizontalAdvance(block.text().mid(sel_start,ri_end-sel_start));
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
                result.m_width+=ri.m_width;
                return result;
            }else{
                if(sel_start<ri.m_start){ //sel_start [ri.start sel_end] ri_end
                    if(ri.m_processId==REPLACED_ID){
                        result.m_width+=fm.horizontalAdvance(ri.m_text_changed.mid(0,sel_end-ri.m_start));
                    }else if(ri.m_processId!=REMOVED_ID){
                        result.m_width+=fm.horizontalAdvance(block.text().mid(ri.m_start,sel_end-ri.m_start));;
                    }
                }else{ // ri.start [sel_start sel_end] ri.end
                    if(ri.m_processId==REPLACED_ID){
                        result.m_width+=fm.horizontalAdvance(block.text().mid(sel_start-ri.m_start,sel_len));
                    }else if(ri.m_processId!=REMOVED_ID){
                        result.m_width+=fm.horizontalAdvance(block.text().mid(sel_start,sel_len));
                    }
                }
            }

            // if selection between removed ri, set mini width to indicated
            if(ri.m_processId==REMOVED_ID && result.m_width==0){
                result.m_width=0.5;
                return result;
            }

            break;
        }

        if(sel_end <= ri.m_start ){// selection end before range,
            break;
        }
    }

    //    qWarning()<<"   selection"<<sel_start<<sel_end<<result.m_width<<block.text().mid(sel_start,sel_len);

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
        selection_off+=result.m_width;
        start=selection->start;
    } else{
        start=li->m_start_new;
    }
    if(selectionEndInLine ){// get width to range end
        len=selection->start+selection->length - start;
        result=getRangesWidth(li,start,len,block);
        selection_width+=result.m_width;
    }else{ // get width to line end
        len=li->m_start_new+li->m_len_new- start;
        result=getRangesWidth(li,start,len,block);
        selection_width+=result.m_width;
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

            QRectF lineRect(tl.naturalTextRect().x(),tl.naturalTextRect().y(),li.m_width,tl.naturalTextRect().height());
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

bool BlockLinesData::atWordSeparator(int& position,QString& text)
{
    const QChar c = text.at(position);
    switch (c.unicode()) {
    case '.':
    case ',':
    case '?':
    case '!':
    case '@':
    case '#':
    case '$':
    case ':':
    case ';':
    case '-':
    case '<':
    case '>':
    case '[':
    case ']':
    case '(':
    case ')':
    case '{':
    case '}':
    case '=':
    case '/':
    case '+':
    case '%':
    case '&':
    case '^':
    case '*':
    case '\'':
    case '"':
    case '`':
    case '~':
    case '|':
    case '\\':
    case 0x2013:
    case 0x2018:
    case 0x2019:
    case 0x2026:
    case 0x3001:
    case 0x3002:
    case 0x201C:
    case 0x201D:
    case 0xFF01:
    case 0xFF0C:
    case 0xFF0E:
    case 0xFF1A:
    case 0xFF1B:
    case 0xFF1F:
//        qWarning()<<"at separator"<<text.mid(position);
        return true;
    default:
        break;
    }
    return false;
}

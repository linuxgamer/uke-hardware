// SPDX-License-Identifier: Apache-2.0
#include "../src/upstream/nt36532-thp/nvt_touch_core.cpp"
#include <cassert>
#include <climits>
#include <iostream>
#include <random>

using Points = std::vector<std::pair<int,int>>;
static auto exhaustive(const Points &predictions,const Points &contacts) {
    const bool transpose=predictions.size()>contacts.size();
    const auto &rows=transpose?contacts:predictions;
    const auto &columns=transpose?predictions:contacts;
    int best=std::numeric_limits<int>::max();
    std::vector<int> selected,winner;
    std::vector<bool> used(columns.size());
    const auto solve=[&](auto &self,int cost) -> void {
        const auto row=selected.size();
        if(row==rows.size()) {
            if(cost<best || (cost==best && selected<winner)) {best=cost;winner=selected;}
            return;
        }
        for(unsigned column=0;column<columns.size();column++)if(!used[column]) {
            used[column]=true;selected.push_back(column);
            self(self,cost+nvt::contactDistance(rows[row].first,rows[row].second,
                                               columns[column].first,columns[column].second));
            selected.pop_back();used[column]=false;
        }
    };
    solve(solve,0);
    std::vector<std::pair<int,int>> result;
    for(unsigned row=0;row<winner.size();row++)
        result.emplace_back(transpose?winner[row]:int(row),transpose?int(row):winner[row]);
    std::sort(result.begin(),result.end());
    return result;
}
int main() {
    assert(nvt::incrementLifetimeCounter(0)==1);
    assert(nvt::incrementLifetimeCounter(INT_MAX-1)==INT_MAX);
    assert(nvt::incrementLifetimeCounter(INT_MAX)==INT_MAX);
    std::array<nvt::TouchCore::TrackerSlot,nvt::kFingerSlots> old{},next{};
    uint16_t cursor=65535;
    old[0].active=true;old[0].tracking_id=65535;
    old[1].active=true;old[1].tracking_id=0;
    next[0].active=true;next[0].tracking_id=1;
    assert(nvt::takeTrackingId(cursor,old,next)==2 && cursor==3);
    old={};next={};cursor=65535;
    assert(nvt::takeTrackingId(cursor,old,next)==65535 && cursor==0);
    assert(nvt::takeTrackingId(cursor,old,next)==0 && cursor==1);
    std::mt19937 random(36532);
    for(unsigned trial=0;trial<160;trial++) {
        Points predictions(1+trial%4),contacts(1+(trial/4)%7);
        for(auto *points:{&predictions,&contacts})for(auto &[x,y]:*points) {
            x=int(random()%7)-3;y=int(random()%7)-3;
        }
        assert(nvt::minimumDistanceAssignment(predictions,contacts)==exhaustive(predictions,contacts));
    }
    assert(nvt::minimumDistanceAssignment({},{{1,2}}).empty());
    assert(nvt::minimumDistanceAssignment({{1,2}},{}).empty());
    for(const auto &[predictions,contacts]:std::array<std::pair<Points,Points>,2>{{
        {Points(13),Points(1)},{Points(1),Points(nvt::kNodes+1)}}}) {
        bool refused=false;
        try {nvt::minimumDistanceAssignment(predictions,contacts);}
        catch(const std::invalid_argument &) {refused=true;}
        assert(refused);
    }
    Points predictions(12),contacts(32);
    for(unsigned index=0;index<contacts.size();index++)contacts[index]={int(index)*100,0};
    for(unsigned index=0;index<predictions.size();index++)predictions[index]=contacts[31-index];
    for(unsigned trial=0;trial<16;trial++) {
        const auto result=nvt::minimumDistanceAssignment(predictions,contacts);
        assert(result.size()==12);
        for(unsigned row=0;row<result.size();row++)assert(result[row]==std::make_pair(int(row),31-int(row)));
    }
    for(auto &point:predictions)point={0,0};
    for(auto &point:contacts)point={0,0};
    const auto tied=nvt::minimumDistanceAssignment(predictions,contacts);
    for(unsigned row=0;row<tied.size();row++)assert(tied[row]==std::make_pair(int(row),int(row)));
    std::cout<<"Exact assignment, transpose, ties and 12-by-32 overload passed\n";
}

/*

*/

#ifdef KARGATUMCORE
#include "KargatumConfig.h"
#else
#include "../Kargatum-lib/LibKargatumConfig.h"
#endif

#include "KargatumCFBG.h"
#include "Log.h"
#include "ScriptMgr.h"
#include "GroupMgr.h"
#include "BattlegroundMgr.h"
#include "Opcodes.h"
#include "Chat.h"

CFBG* CFBG::instance()
{
    static CFBG instance;
    return &instance;
}

bool CFBG::IsSystemEnable()
{
    return CONF_BOOL(conf::CFBG_ENABLE);
}

uint32 CFBG::GetBGTeamAverageItemLevel(Battleground* bg, TeamId team)
{
    if (!bg)
        return 0;

    uint32 PlayersCount = bg->GetPlayersCountByTeam(team);
    if (!PlayersCount)
        return 0;

    uint32 Sum = 0;
    uint32 Count = 0;

    Battleground::BattlegroundPlayerMap const& pl = bg->GetPlayers();
    for (Battleground::BattlegroundPlayerMap::const_iterator itr = pl.begin(); itr != pl.end(); ++itr)
    {
        Player* plr = itr->second;
        if (!plr)
            continue;

        if (plr->GetTeamId(true) != team)
            continue;

        Sum += plr->GetAverageItemLevel();
        Count++;
    }

    if (!Count || !Sum)
        return 0;

    return Sum / Count;
}

TeamId CFBG::GetLowerTeamIdInBG(Battleground* bg)
{
    int32 PlCountA = bg->GetPlayersCountByTeam(TEAM_ALLIANCE);
    int32 PlCountH = bg->GetPlayersCountByTeam(TEAM_HORDE);
    uint32 Diff = abs(PlCountA - PlCountH);

    if (Diff)
        return PlCountA < PlCountH ? TEAM_ALLIANCE : TEAM_HORDE;

    if (CONF_BOOL(conf::CFBG_INCLUDE_AVG_ILVL_ENABLE) && !this->IsAvgIlvlTeamsInBgEqual(bg))
        return this->GetLowerAvgIlvlTeamInBg(bg);

    uint8 rnd = urand(0, 1);

    if (rnd)
        return TEAM_ALLIANCE;

    return TEAM_HORDE;
}

TeamId CFBG::GetLowerAvgIlvlTeamInBg(Battleground* bg)
{
    uint32 AvgAlliance = this->GetBGTeamAverageItemLevel(bg, TeamId::TEAM_ALLIANCE);
    uint32 AvgHorde = this->GetBGTeamAverageItemLevel(bg, TeamId::TEAM_HORDE);

    return (AvgAlliance < AvgHorde) ? TEAM_ALLIANCE : TEAM_HORDE;
}

bool CFBG::IsAvgIlvlTeamsInBgEqual(Battleground* bg)
{
    uint32 AvgAlliance = this->GetBGTeamAverageItemLevel(bg, TeamId::TEAM_ALLIANCE);
    uint32 AvgHorde = this->GetBGTeamAverageItemLevel(bg, TeamId::TEAM_HORDE);

    return AvgAlliance == AvgHorde;
}

void CFBG::ValidatePlayerForBG(Battleground* bg, Player* player, TeamId teamId)
{
    player->SetBGTeamID(teamId);

    this->SetFakeRaceAndMorph(player);

    float x, y, z, o;
    bg->GetTeamStartLoc(teamId, x, y, z, o);
    player->TeleportTo(bg->GetMapId(), x, y, z, o);
}

uint32 CFBG::GetAllPlayersCountInBG(Battleground* bg)
{
    //int32 PlCountA = bg->GetPlayersCountByTeam(TEAM_ALLIANCE);
    //int32 PlCountH = bg->GetPlayersCountByTeam(TEAM_HORDE);

    return bg->GetPlayersSize();
}

void CFBG::SetFakeRaceAndMorph(Player* player)
{
    if (!player->InBattleground())
        return;

    if (player->GetTeamId(true) == player->GetBgTeamId())
        return;

    uint8 FakeRace;
    uint32 FakeMorph;

    if (player->getClass() == CLASS_DRUID)
    {
        if (player->GetTeamId(true) == TEAM_ALLIANCE)
        {
            FakeMorph = player->getGender() == GENDER_MALE ? FAKE_M_TAUREN : FAKE_F_TAUREN;
            FakeRace = RACE_TAUREN;
        }
        else if (player->getGender() == GENDER_MALE) // HORDE PLAYER, ONLY HAVE MALE Night Elf ID
        {
            FakeMorph = FAKE_M_NELF;
            FakeRace = RACE_NIGHTELF;
        }
        else
            FakeRace = player->GetTeamId(true) == TEAM_ALLIANCE ? RACE_BLOODELF : RACE_HUMAN;

        if (player->GetTeamId(true) == TEAM_HORDE)
        {
            if (player->getGender() == GENDER_MALE)
                FakeMorph = 19723;
            else
                FakeMorph = 19724;
        }
        else
        {
            if (player->getGender() == GENDER_MALE)
                FakeMorph = 20578;
            else
                FakeMorph = 20579;
        }
    }
    else if (player->getClass() == CLASS_SHAMAN && player->GetTeamId(true) == TEAM_HORDE && player->getGender() == GENDER_FEMALE)
    {
        FakeMorph = FAKE_F_DRAENEI; // Female Draenei
        FakeRace = RACE_DRAENEI;
    }
    else
    {
        FakeRace = player->GetTeamId(true) == TEAM_ALLIANCE ? RACE_BLOODELF : RACE_HUMAN;

        if (player->GetTeamId(true) == TEAM_HORDE)
        {
            if (player->getGender() == GENDER_MALE)
                FakeMorph = 19723;
            else
                FakeMorph = 19724;
        }
        else
        {
            if (player->getGender() == GENDER_MALE)
                FakeMorph = 20578;
            else
                FakeMorph = 20579;
        }
    }

    TeamId FakeTeamID = player->TeamIdForRace(FakeRace);

    FakePlayer fakePlayer;
    fakePlayer.FakeMorph    = FakeMorph;
    fakePlayer.FakeRace     = FakeRace;
    fakePlayer.FakeTeamID   = FakeTeamID;
    fakePlayer.RealMorph    = player->GetDisplayId();
    fakePlayer.RealRace     = player->getRace(true);
    fakePlayer.RealTeamID   = player->GetTeamId(true);

    _fakePlayerStore[player] = fakePlayer;

    player->setRace(FakeRace);
    player->setFactionForRace(FakeRace);
    player->SetDisplayId(FakeMorph);
    player->SetNativeDisplayId(FakeMorph);
}

void CFBG::ClearFakePlayer(Player* player)
{
    if (!this->IsPlayerFake(player))
        return;

    player->setRace(_fakePlayerStore[player].RealRace);
    player->setFactionForRace(_fakePlayerStore[player].RealRace);
    player->SetDisplayId(_fakePlayerStore[player].RealMorph);
    player->SetNativeDisplayId(_fakePlayerStore[player].RealMorph);

    _fakePlayerStore.erase(player);
}

bool CFBG::IsPlayerFake(Player* player)
{
    FakePlayersContainer::const_iterator itr = _fakePlayerStore.find(player);
    if (itr != _fakePlayerStore.end())
        return true;

    return false;
}

void CFBG::DoForgetPlayersInList(Player* player)
{
    // m_FakePlayers is filled from a vector within the battleground
    // they were in previously so all players that have been in that BG will be invalidated.
    for (auto itr : _fakeNamePlayersStore)
    {
        WorldPacket data(SMSG_INVALIDATE_PLAYER, 8);
        data << itr.second;
        player->GetSession()->SendPacket(&data);

        if (Player* _player = ObjectAccessor::FindPlayer(itr.second))
            player->GetSession()->SendNameQueryOpcode(_player->GetGUID());
    }
        
    _fakeNamePlayersStore.erase(player);
}

void CFBG::FitPlayerInTeam(Player* player, bool action, Battleground* bg)
{
    if (!bg)
        bg = player->GetBattleground();

    if ((!bg || bg->isArena()) && action)
        return;

    if (action)
        SetForgetBGPlayers(player, true);
    else
        SetForgetInListPlayers(player, true);
}

void CFBG::SetForgetBGPlayers(Player* player, bool value)
{
    _forgetBGPlayersStore[player] = value;
}

bool CFBG::ShouldForgetBGPlayers(Player* player)
{
    return _forgetBGPlayersStore[player];
}

void CFBG::SetForgetInListPlayers(Player* player, bool value)
{
    _forgetInListPlayersStore[player] = value;
}

bool CFBG::ShouldForgetInListPlayers(Player* player)
{
    return _forgetInListPlayersStore[player];
}

void CFBG::DoForgetPlayersInBG(Player* player, Battleground* bg)
{
    for (Battleground::BattlegroundPlayerMap::const_iterator itr = bg->GetPlayers().begin(); itr != bg->GetPlayers().end(); ++itr)
    {
        // Here we invalidate players in the bg to the added player
        WorldPacket data1(SMSG_INVALIDATE_PLAYER, 8);
        data1 << itr->first;
        player->GetSession()->SendPacket(&data1);

        if (Player * pPlayer = ObjectAccessor::FindPlayer(itr->first))
        {
            player->GetSession()->SendNameQueryOpcode(pPlayer->GetGUID()); // Send namequery answer instantly if player is available

            // Here we invalidate the player added to players in the bg
            WorldPacket data2(SMSG_INVALIDATE_PLAYER, 8);
            data2 << player->GetGUID();
            pPlayer->GetSession()->SendPacket(&data2);
            pPlayer->GetSession()->SendNameQueryOpcode(player->GetGUID());
        }
    }
}

bool CFBG::SendRealNameQuery(Player* player)
{
    if (this->IsPlayingNative(player))
        return false;

    WorldPacket data(SMSG_NAME_QUERY_RESPONSE, (8 + 1 + 1 + 1 + 1 + 1 + 10));
    data.appendPackGUID(player->GetGUID());                             // player guid
    data << uint8(0);                                           // added in 3.1; if > 1, then end of packet
    data << player->GetName();                                          // played name
    data << uint8(0);                                           // realm name for cross realm BG usage
    data << uint8(player->getRace(true));
    data << uint8(player->getGender());
    data << uint8(player->getClass());
    data << uint8(0);                                           // is not declined
    player->GetSession()->SendPacket(&data);

    return true;
}

bool CFBG::IsPlayingNative(Player* player)
{
    return player->GetTeamId(true) == player->GetBGData().bgTeamId;
}

bool CFBG::CheckCrossFactionMatch(BattlegroundQueue* bgqueue, BattlegroundBracketId bracket_id, Battleground* bg)
{
    if (!this->IsSystemEnable() || bg->isArena())
        return false; // Only do this if crossbg's are enabled.

    // Here we will add all players to selectionpool, later we check if there are enough and launch a bg.
    FillXPlayersToBG(bgqueue, bracket_id, bg, true);

    if (sBattlegroundMgr->isTesting() && (bgqueue->m_SelectionPools[TEAM_ALLIANCE].GetPlayerCount() || bgqueue->m_SelectionPools[TEAM_HORDE].GetPlayerCount()))
        return true;

    uint32 qPlayers = 0;

    for (BattlegroundQueue::GroupsQueueType::const_iterator itr = bgqueue->m_QueuedGroups[bracket_id][BG_QUEUE_CFBG].begin(); itr != bgqueue->m_QueuedGroups[bracket_id][BG_QUEUE_CFBG].end(); ++itr)
        if (!(*itr)->IsInvitedToBGInstanceGUID)
            qPlayers += (*itr)->Players.size();

    uint8 MPT = bg->GetMinPlayersPerTeam() * 2;
    if (qPlayers < MPT)
        return false;

    return true;
}

bool CFBG::FillXPlayersToBG(BattlegroundQueue* bgqueue, BattlegroundBracketId bracket_id, Battleground* bg, bool start)
{
    uint8 queuedPeople = 0;
    for (BattlegroundQueue::GroupsQueueType::const_iterator itr = bgqueue->m_QueuedGroups[bracket_id][BG_QUEUE_CFBG].begin(); itr != bgqueue->m_QueuedGroups[bracket_id][BG_QUEUE_CFBG].end(); ++itr)
        if (!(*itr)->IsInvitedToBGInstanceGUID)
            queuedPeople += (*itr)->Players.size();

    if (this->IsSystemEnable() && (sBattlegroundMgr->isTesting() || queuedPeople >= bg->GetMinPlayersPerTeam() * 2 || !start))
    {
        int32 aliFree = start ? bg->GetMaxPlayersPerTeam() : bg->GetFreeSlotsForTeam(TEAM_ALLIANCE);
        int32 hordeFree = start ? bg->GetMaxPlayersPerTeam() : bg->GetFreeSlotsForTeam(TEAM_HORDE);
        // Empty selection pools. They will be refilled from queued groups.
        bgqueue->m_SelectionPools[TEAM_ALLIANCE].Init();
        bgqueue->m_SelectionPools[TEAM_HORDE].Init();
        int32 valiFree = aliFree;
        int32 vhordeFree = hordeFree;
        int32 diff = 0;

        // Add teams to their own factions as far as possible.
        if (start)
        {
            QueuedGroupMap m_PreGroupMap_a, m_PreGroupMap_h;
            int32 m_SmallestOfTeams = 0;
            int32 queuedAlliance = 0;
            int32 queuedHorde = 0;

            for (BattlegroundQueue::GroupsQueueType::const_iterator itr = bgqueue->m_QueuedGroups[bracket_id][BG_QUEUE_CFBG].begin(); itr != bgqueue->m_QueuedGroups[bracket_id][BG_QUEUE_CFBG].end(); ++itr)
            {
                if ((*itr)->IsInvitedToBGInstanceGUID)
                    continue;

                bool alliance = (*itr)->RealTeamID == TEAM_ALLIANCE;

                if (alliance)
                {
                    m_PreGroupMap_a.insert(std::make_pair((*itr)->Players.size(), *itr));
                    queuedAlliance += (*itr)->Players.size();
                }
                else
                {
                    m_PreGroupMap_h.insert(std::make_pair((*itr)->Players.size(), *itr));
                    queuedHorde += (*itr)->Players.size();
                }
            }

            m_SmallestOfTeams = std::min(std::min(aliFree, queuedAlliance), std::min(hordeFree, queuedHorde));

            valiFree -= PreAddPlayers(bgqueue, m_PreGroupMap_a, m_SmallestOfTeams, aliFree);
            vhordeFree -= PreAddPlayers(bgqueue, m_PreGroupMap_h, m_SmallestOfTeams, hordeFree);
        }        

        for (BattlegroundQueue::GroupsQueueType::const_iterator itr = bgqueue->m_QueuedGroups[bracket_id][BG_QUEUE_CFBG].begin(); itr != bgqueue->m_QueuedGroups[bracket_id][BG_QUEUE_CFBG].end(); ++itr)
            m_QueuedGroupMap.insert(std::make_pair((*itr)->Players.size(), *itr));

        for (QueuedGroupMap::reverse_iterator itr = m_QueuedGroupMap.rbegin(); itr != m_QueuedGroupMap.rend(); ++itr)
        {
            BattlegroundQueue::GroupsQueueType allypool = bgqueue->m_SelectionPools[TEAM_ALLIANCE].SelectedGroups;
            BattlegroundQueue::GroupsQueueType hordepool = bgqueue->m_SelectionPools[TEAM_HORDE].SelectedGroups;

            GroupQueueInfo* ginfo = itr->second;

            // If player already was invited via pre adding (add to own team first) or he was already invited to a bg, skip.
            if (ginfo->IsInvitedToBGInstanceGUID ||
                std::find(allypool.begin(), allypool.end(), ginfo) != allypool.end() ||
                std::find(hordepool.begin(), hordepool.end(), ginfo) != hordepool.end() ||
                (bgqueue->m_SelectionPools[TEAM_ALLIANCE].GetPlayerCount() >= bg->GetMinPlayersPerTeam() &&
                    bgqueue->m_SelectionPools[TEAM_HORDE].GetPlayerCount() >= bg->GetMinPlayersPerTeam()))
                continue;

            diff = abs(valiFree - vhordeFree);
            bool moreAli = valiFree < vhordeFree;

            if (diff > 0)
                ginfo->teamId = moreAli ? TEAM_ALLIANCE : TEAM_HORDE;

            bool alliance = ginfo->teamId == TEAM_ALLIANCE;

            if (bgqueue->m_SelectionPools[alliance ? TEAM_ALLIANCE : TEAM_HORDE].AddGroup(ginfo, alliance ? aliFree : hordeFree))
                alliance ? valiFree -= ginfo->Players.size() : vhordeFree -= ginfo->Players.size();
        }

        return true;
    }

    return false;
}

int32 CFBG::PreAddPlayers(BattlegroundQueue* bgqueue, QueuedGroupMap m_PreGroupMap, int32 MaxAdd, uint32 MaxInTeam)
{
    int32 LeftToAdd = MaxAdd;
    uint32 Added = 0;

    for (QueuedGroupMap::reverse_iterator itr = m_PreGroupMap.rbegin(); itr != m_PreGroupMap.rend(); ++itr)
    {
        int32 PlayerSize = itr->first;
        bool alliance = itr->second->RealTeamID == TEAM_ALLIANCE;

        if (PlayerSize <= LeftToAdd && bgqueue->m_SelectionPools[alliance ? TEAM_ALLIANCE : TEAM_HORDE].AddGroup(itr->second, MaxInTeam))
            LeftToAdd -= PlayerSize, Added -= PlayerSize;
    }

    return LeftToAdd;
}

void CFBG::UpdateForget(Player* player)
{
    Battleground* bg = player->GetBattleground();
    if (bg)
    {
        if (this->ShouldForgetBGPlayers(player) && bg)
        {
            this->DoForgetPlayersInBG(player, bg);
            this->SetForgetBGPlayers(player, false);
        }
    }
    else if (this->ShouldForgetInListPlayers(player))
    {
        this->DoForgetPlayersInList(player);
        this->SetForgetInListPlayers(player, false);
    }
}

bool CFBG::SendMessageQueue(BattlegroundQueue* bgqueue, Battleground* bg, PvPDifficultyEntry const* bracketEntry, Player* leader)
{
    if (!this->IsSystemEnable())
        return false;

    BattlegroundBracketId bracketId = bracketEntry->GetBracketId();

    char const* bgName = bg->GetName();
    uint32 q_min_level = std::min(bracketEntry->minLevel, (uint32)80);
    uint32 q_max_level = std::min(bracketEntry->maxLevel, (uint32)80);
    uint32 qHorde = 0;
    uint32 qAlliance = 0;

    uint32 MinPlayers = bg->GetMinPlayersPerTeam() * 2;
    uint32 qPlayers = 0;

    for (BattlegroundQueue::GroupsQueueType::const_iterator itr = bgqueue->m_QueuedGroups[bracketId][BG_QUEUE_CFBG].begin(); itr != bgqueue->m_QueuedGroups[bracketId][BG_QUEUE_CFBG].end(); ++itr)
        if (!(*itr)->IsInvitedToBGInstanceGUID)
            qPlayers += (*itr)->Players.size();

    ChatHandler(leader->GetSession()).PSendSysMessage("БГ %s (Уровни: %u - %u). Зарегистрировано: %u/%u", bgName, q_min_level, q_max_level, qPlayers, MinPlayers);

    return true;
}

// Kargatum_CFBG SC
class Kargatum_CFBG : public BGScript
{
public:
    Kargatum_CFBG() : BGScript("Kargatum_CFBG") {}

    void OnBattlegroundBeforeAddPlayer(Battleground* bg, Player* player) override
    {
        if (!bg || bg->isArena() || !player)
            return;

        if (!sCFBG->IsSystemEnable())
            return;

        TeamId teamid;
        Group* group = player->GetOriginalGroup();
        uint32 PlayerCountInBG = sCFBG->GetAllPlayersCountInBG(bg);

        PlayerCountInBG ? (teamid = sCFBG->GetLowerTeamIdInBG(bg)) : (teamid = player->GetTeamId(true));

        if (!group)
            sCFBG->ValidatePlayerForBG(bg, player, teamid);
        else
        {
            for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (!member)
                    continue;

                if (bg->IsPlayerInBattleground(member->GetGUID()))
                    continue;

                sCFBG->ValidatePlayerForBG(bg, member, teamid);
            }
        }
    }

    void OnBattlegroundAddPlayer(Battleground* bg, Player* player) override
    {
        if (!sCFBG->IsSystemEnable())
            return;

        // Kargatum CFBG
        sCFBG->FitPlayerInTeam(player, true, bg);
    }

    void OnBattlegroundEndReward(Battleground* bg, Player* player, TeamId /*winnerTeamId*/) override
    {
        if (!bg || !player || bg->isArena())
            return;

        if (!sCFBG->IsSystemEnable())
            return;

        if (sCFBG->IsPlayerFake(player))
            sCFBG->ClearFakePlayer(player);
    }

    void OnBattlegroundRemovePlayerAtLeave(Battleground* bg, Player* player) override
    {
        if (!sCFBG->IsSystemEnable())
            return;

        // Kargatum CFBG
        sCFBG->FitPlayerInTeam(player, false, bg);

        if (sCFBG->IsPlayerFake(player))
            sCFBG->ClearFakePlayer(player);
    }
};

class Kargatum_CFBG_Player : public PlayerScript
{
public:
    Kargatum_CFBG_Player() : PlayerScript("Kargatum_CFBG_Player") { }
    
    void OnLogin(Player* player) override
    {
        if (!sCFBG->IsSystemEnable())
            return;

        if (player->GetTeamId(true) != player->GetBgTeamId())
            sCFBG->FitPlayerInTeam(player, player->GetBattleground() && !player->GetBattleground()->isArena() ? true : false, player->GetBattleground());
    }
};

void AddSC_Kargatum_CFBG()
{
    new Kargatum_CFBG();
    new Kargatum_CFBG_Player();
}

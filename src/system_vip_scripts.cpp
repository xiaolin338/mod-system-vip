/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "SystemVip.h"
#include "WorldSessionMgr.h"

#define sV sSystemVip

class SystemVipPlayer : public PlayerScript
{
public:
    SystemVipPlayer() : PlayerScript("SystemVipPlayer", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_GIVE_EXP,
        PLAYERHOOK_ON_BEFORE_LOOT_MONEY,
        PLAYERHOOK_ON_PLAYER_RELEASED_GHOST,
        PLAYERHOOK_ON_PLAYER_RESURRECT,
        PLAYERHOOK_ON_VICTIM_REWARD_AFTER
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (sConfigMgr->GetOption<bool>("SystemVip.Announce", false))
            ChatHandler(player->GetSession()).SendSysMessage("当前服务器|cff4CFF00 Vip |r系统已启用.");

        if (sV->isVip(player) && sV->loginAnnounce)
            sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, sV->getLoginMessage(player));

        if (sV->isVip(player))
        {
            std::string remaining = sV->getFormatedVipTime(player);
            std::string msg = "VIP订阅剩余时间: |cff4CFF00" + remaining + "|r";
            ChatHandler(player->GetSession()).SendSysMessage(msg.c_str());

            if (!player->HasItemCount(44824, 1, true))
            {
                player->AddItem(44824, 1);
            }
        }

        sV->delExpireVip(player);
        if (sV->saveTeleport && sV->isVip(player))
            sV->loadTeleportVip(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (sV->saveTeleport && sV->isVip(player))
            sV->teleportMap.erase(player->GetSession()->GetAccountId());
    }

    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* /*victim*/, uint8 /*xpSource*/) override
    {
        if (sV->isVip(player) && sV->rateCustom)
            amount *= sV->rateXp;
    }

    void OnPlayerBeforeLootMoney(Player* player, Loot* loot) override
    {
        if (sV->isVip(player) && sV->rateCustom)
            loot->gold *= sV->goldRate;
    }

    void OnPlayerReleasedGhost(Player* player) override
    {
        if (sV->isVip(player) && sV->ghostMount)
        {
            if (!player->HasAura(551640))
                player->AddAura(551640, player);
        }
    }

    void OnPlayerResurrect(Player* player, float /*restorePercent*/, bool& /*applySickness*/) override
    {
        if (player->HasAura(551640))
        {
            player->RemoveAura(551640);
        }
    }

    void OnPlayerVictimRewardAfter(Player* player, Player* /*victim*/, uint32& /*killer_title*/, int32& /*victim_rank*/, float& honor_f) override
    {
        if (sV->isVip(player) && sV->rateCustom)
        {
            if (sV->honorRate > 1)
                player->ModifyHonorPoints((honor_f * sV->honorRate) - honor_f);
        }
    }
};

class SystemVipVendor : public CreatureScript {
public:
    SystemVipVendor() : CreatureScript("SystemVipVendor") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/ICONS/INV_Misc_Coin_02:28:28:-15:0|t VIP订阅.", 0, 1,
            "你想要订阅"+ to_string(sV->TimeVip / 86400) + "天VIP系统吗？\n价格: " + to_string(sV->TokenAmount) + "\n " + sV->TokenIcon + " " + sV->getItemLink(sV->TokenEntry, player), 0, false);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/ICONS/INV_Misc_QuestionMark:28:28:-15:0|t 信息.", 0, 2);
        if (sV->isVip(player))
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/ICONS/ability_hunter_beastcall:28:28:-15:0|t 取回我的VIP宠物.", 0, 4);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/ICONS/Trade_Engineering:28:28:-15:0|t 关闭.", 0, 3);
        SendGossipMenuFor(player, 1, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        ClearGossipMenuFor(player);
        switch (action)
        {
            case 1:
                if (player->HasItemCount(sV->TokenEntry, sV->TokenAmount))
                {
                    player->DestroyItemCount(sV->TokenEntry, sV->TokenAmount, true);
                    sV->addRemainingVipTime(player);
                    if (!player->HasItemCount(44824, 1, true))
                        player->AddItem(44824, 1);

                    // ChatHandler(player->GetSession()).PSendSysMessage("感谢你的VIP订阅.");
                    // ChatHandler(player->GetSession()).PSendSysMessage("VIP订阅剩余时间: %s", sV->getFormatedVipTime(player).c_str());
                    
                    std::string remaining = sV->getFormatedVipTime(player);
                    std::string msg = "VIP订阅剩余时间: |cff4CFF00" + remaining + "|r";
                    ChatHandler(player->GetSession()).SendSysMessage(msg.c_str());
            
                    OnGossipSelect(player, creature, 0, 2);
                }
                else
                {
                    ChatHandler(player->GetSession()).PSendSysMessage("你的代币不足.");
                    CloseGossipMenuFor(player);
                }
                break;
            case 2:
                sV->sendGossipInformation(player, true);
                AddGossipItemFor(player, 0, "|TInterface/ICONS/Trade_Engineering:28:28:-15:0|t 关闭.", 0, 3);
                SendGossipMenuFor(player, VENDOR_INFO, creature->GetGUID());
                break;
            case 4:
                if (!player->HasItemCount(44824, 1, true))
                {
                    player->AddItem(44824, 1);
                    creature->Whisper("别再弄丢了.", LANG_UNIVERSAL, player, false);
                    CloseGossipMenuFor(player);
                }
                else
                {
                    creature->Whisper("你已经有了召唤VIP宠物的物品.", LANG_UNIVERSAL, player, false);
                    OnGossipHello(player, creature);
                }
                break;
            default:
                CloseGossipMenuFor(player);
                break;
        }
        return true;
    }
};

class SystemVipPocket : ItemScript {
public:
    SystemVipPocket() : ItemScript("SystemVipPocket") {}

    bool OnUse(Player* player, Item* /*item*/, SpellCastTargets const& /*targets*/) override
    {
        if (!sV->isVip(player))
        {
            // ChatHandler(player->GetSession()).PSendSysMessage("你不是VIP！");
            ChatHandler(player->GetSession()).PSendSysMessage("你还没有订阅VIP服务或服务已过期,登录官网个人中心订阅!");
            return false;
        }

        if (player->GetMap()->IsBattleArena())
        {
            ChatHandler(player->GetSession()).PSendSysMessage("你不能在竞技场中使用!");
            return false;
        }

        player->CastSpell(player, 73213);
        player->PlayDistanceSound(3980, player);

        float distance = 15;//宠物距离
        float angle = player->GetOrientation() * M_PI / 180.0f;

        Creature* pet = player->SummonCreature(100043,
            player->GetPositionX() + (distance * cos(angle)),
            player->GetPositionY() + (distance * sin(angle)),
            player->GetPositionZ(), player->GetOrientation(),
            TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 60000);
        pet->GetMotionMaster()->MoveFollow(player, PET_FOLLOW_DIST + 2.0, PET_FOLLOW_ANGLE);
        pet->SetFaction(player->GetFaction());
        pet->SetLevel(player->GetLevel());
        pet->SetCreatorGUID(player->GetGUID());
        return false;
    }
};

class SystemVipPet : CreatureScript {
public:
    SystemVipPet() : CreatureScript("SystemVipPet") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);
        sV->sendGossipInformation(player, false);
        if (creature->GetCreatorGUID() != player->GetGUID())
            return true;

        if (!sV->petEnable)
            return true;

        if (sV->vipZone)
            AddGossipItemFor(player, 0, "|TInterface/ICONS/Achievement_Zone_ZulDrak_12:28:28:-15:0|t VIP区域", 0, 1);
        if (sV->armorRep)
            AddGossipItemFor(player, 0, "|TInterface/ICONS/INV_Hammer_20:28:28:-15:0|t 修理装备.", 0, 2);
        if (sV->bankEnable)
            AddGossipItemFor(player, 0, "|TInterface/ICONS/INV_Ingot_03:28:28:-15:0|t 我的银行.", 0, 3);
        if (sV->mailEnable)
            AddGossipItemFor(player, 0, "|TInterface/ICONS/inv_letter_15:28:28:-15:0|t 我的邮箱.", 0, 8);
        if (sV->buffsEnable)
        {
            AddGossipItemFor(player, 0, "|TInterface/ICONS/Spell_Magic_GreaterBlessingofKings:28:28:-15:0|t 增益", 0, 4);
            AddGossipItemFor(player, 0, "|TInterface/PAPERDOLLINFOFRAME/UI-GearManager-Undo:28:28:-15:0|t 移除增益", 0, 11);
        }
        if (sV->refreshEnable)
            AddGossipItemFor(player, 0, "|TInterface/ICONS/Spell_Holy_LayOnHands:28:28:-15:0|t 恢复生命/法力.", 0, 5);
        if (sV->sicknessEnbale)
            AddGossipItemFor(player, 0, "|TInterface/ICONS/spell_shadow_deathscream:28:28:-15:0|t 移除疾病.", 0, 6);
        if (sV->deserterEnable)
            AddGossipItemFor(player, 0, "|TInterface/ICONS/ability_druid_cower:28:28:-15:0|t 移除逃亡者.", 0, 7);
        if (sV->resetInstance)
            AddGossipItemFor(player, 0, "|TInterface/ICONS/Achievement_Dungeon_Icecrown_IcecrownEntrance:28:28:-15:0|t 重置副本.", 0, 9);
        if (sV->saveTeleport)
            AddGossipItemFor(player, 0, "|TInterface/ICONS/Spell_Holy_LightsGrace:28:28:-15:0|t 我的传送.", 0, 10);
        AddGossipItemFor(player, 0, "|TInterface/ICONS/Trade_Engineering:28:28:-15:0|t 关闭.", 0, 100);

        SendGossipMenuFor(player, PET_INFO, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        ClearGossipMenuFor(player);
        switch (action)
        {
            case 1:
                if (player->IsInCombat())
                {
                    ChatHandler(player->GetSession()).PSendSysMessage("你正在战斗中！");
                    CloseGossipMenuFor(player);
                }
                else
                {
                    player->TeleportTo(sV->vipZoneMapId, sV->vipZonePosX, sV->vipZonePosY, sV->vipZonePosZ, sV->vipZoneO);
                    CloseGossipMenuFor(player);
                }
                break;
            case 2:
                player->DurabilityRepairAll(false, 0, false);
                ChatHandler(player->GetSession()).PSendSysMessage("你修理了装备.");
                OnGossipHello(player, creature);
                break;
            case 3:
                creature->SetUInt32Value(UNIT_NPC_FLAGS, creature->GetUInt32Value(UNIT_NPC_FLAGS) | UNIT_NPC_FLAG_BANKER);
                player->GetSession()->SendShowBank(creature->GetGUID());
                break;
            case 4:
                for (size_t i = 0; i < sV->buffIds.size(); i++)
                    player->AddAura(sV->buffIds[i], player);
                player->CastSpell(player, 16609);
                ChatHandler(player->GetSession()).PSendSysMessage("给你增益效果！");
                OnGossipHello(player, creature);
                break;
            case 5:
                if (player->IsInCombat())
                {
                    CloseGossipMenuFor(player);
                    ChatHandler(player->GetSession()).PSendSysMessage("你正在战斗中！");
                    return false;
                }
                else if (player->getPowerType() == POWER_MANA)
                    player->SetPower(POWER_MANA, player->GetMaxPower(POWER_MANA));
                player->SetHealth(player->GetMaxHealth());
                ChatHandler(player->GetSession()).PSendSysMessage("生命/法力已恢复！");
                creature->CastSpell(player, 31726, true);
                OnGossipHello(player, creature);
                break;
            case 6:
                if (player->HasAura(15007))
                    player->RemoveAura(15007);
                ChatHandler(player->GetSession()).PSendSysMessage("你的疾病已被移除.");
                creature->CastSpell(player, 31726, true);
                OnGossipHello(player, creature);
                break;
            case 7:
                if (player->HasAura(26013))
                    player->RemoveAura(26013);
                ChatHandler(player->GetSession()).PSendSysMessage("你的逃亡者标记已被移除.");
                creature->CastSpell(player, 31726);
                OnGossipHello(player, creature);
                break;
            case 8:
                creature->SetUInt32Value(UNIT_NPC_FLAGS, creature->GetUInt32Value(UNIT_NPC_FLAGS) | UNIT_NPC_FLAG_MAILBOX);
                CloseGossipMenuFor(player);
                player->GetSession()->SendShowMailBox(creature->GetGUID());
                break;
            case 9:
                for (uint8 i = 0; i < MAX_DIFFICULTY; ++i)
                {
                    BoundInstancesMap const& m_boundInstances = sInstanceSaveMgr->PlayerGetBoundInstances(player->GetGUID(), Difficulty(i));
                    for (BoundInstancesMap::const_iterator itr = m_boundInstances.begin(); itr != m_boundInstances.end();)
                    {
                        if (itr->first != player->GetMapId())
                        {
                            sInstanceSaveMgr->PlayerUnbindInstance(player->GetGUID(), itr->first, Difficulty(i), true, player);
                            itr = m_boundInstances.begin();
                        }
                        else
                            ++itr;
                    }
                }
                ChatHandler(player->GetSession()).PSendSysMessage("你的副本已被重置！");
                creature->CastSpell(player, 59908);
                OnGossipHello(player, creature);
                return true;
            case 11:
                for (size_t i = 0; i < sV->buffIds.size(); i++)
                {
                    if (player->HasAura(sV->buffIds[i]))
                        player->RemoveAura(sV->buffIds[i]);
                }
                if (player->HasAura(16609))
                    player->RemoveAura(16609);
                creature->CastSpell(player, 31726);
                ChatHandler(player->GetSession()).PSendSysMessage("增益效果已移除.");
                OnGossipHello(player, creature);
                break;
            case 10:
                AddGossipItemFor(player, 0, "|TInterface/GUILDBANKFRAME/UI-GuildBankFrame-NewTab:28:28:-15:0|t 添加新的.", 0, 1, "用于保存坐标的名称.", 0, true);
                AddGossipItemFor(player, 0, "|TInterface/PAPERDOLLINFOFRAME/UI-GearManager-Undo:28:28:-15:0|t 删除.", 0, 2, "要删除的名称.", 0, true);
                sV->getTeleports(player);
                SendGossipMenuFor(player, 1, creature->GetGUID());
                break;
            case 12:
                sV->teleportPlayer(player, sender);
                CloseGossipMenuFor(player);
                break;
            default:
                CloseGossipMenuFor(player);
                break;
        }
        return true;
    }

    bool OnGossipSelectCode(Player* player, Creature* creature, uint32 /*sender*/, uint32 action, const char* code) override
    {
        switch (action)
        {
            case 1:
                sV->addTeleportVip(player, code);
                OnGossipSelect(player, creature, 0, 10);
                break;
            case 2:
                sV->delTeleportVip(player, code);
                OnGossipSelect(player, creature, 0, 10);
                break;
            default:
                CloseGossipMenuFor(player);
                break;
        }
        return true;
    }
};

class SystemVipWorld : WorldScript {
public:
    SystemVipWorld() : WorldScript("SystemVipWorld", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD,
        WORLDHOOK_ON_UPDATE
    }) {}

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        sV->LoadConfig();
        sV->LoadVipDataFromDB();
    }

    void OnUpdate(uint32 /*diff*/) override
    {
        // 开关未启用或间隔为0时不更新
        if (!sV->EnableAutoUpdate || sV->UpdateInterval == 0)
            return;

        time_t now = time(nullptr);
        if (now - sV->LastUpdateTime >= sV->UpdateInterval)
        {
            sV->LoadVipDataFromDB();
        }
    }
};

void Addsystem_vipScripts()
{
    new SystemVipPlayer();
    new SystemVipVendor();
    new SystemVipWorld();
    new SystemVipPocket();
    new SystemVipPet();
}

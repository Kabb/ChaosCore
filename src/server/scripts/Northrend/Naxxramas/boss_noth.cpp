/*
 * Copyright (C) 2008-2011 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptPCH.h"
#include "naxxramas.h"

#define SAY_AGGRO               RAND(-1533075, -1533076, -1533077)
#define SAY_SUMMON              -1533078
#define SAY_SLAY                RAND(-1533079, -1533080)
#define SAY_DEATH               -1533081

#define SOUND_DEATH      8848

#define SPELL_CURSE_PLAGUEBRINGER       RAID_MODE(29213, 54835)
#define SPELL_BLINK                     RAND(29208, 29209, 29210, 29211)
#define SPELL_CRIPPLE                   RAID_MODE(29212, 54814)
#define SPELL_TELEPORT                  29216
#define SPELL_BERSERK                   27680

#define MOB_WARRIOR         16984
#define MOB_CHAMPION        16983
#define MOB_GUARDIAN        16981

// Teleport position of Noth on his balcony
#define TELE_X 2631.370f
#define TELE_Y -3529.680f
#define TELE_Z 274.040f
#define TELE_O 6.277f

#define MAX_SUMMON_POS 5

const float SummonPos[MAX_SUMMON_POS][4] =
{
    {2728.12f, -3544.43f, 261.91f, 6.04f},
    {2729.05f, -3544.47f, 261.91f, 5.58f},
    {2728.24f, -3465.08f, 264.20f, 3.56f},
    {2704.11f, -3456.81f, 265.53f, 4.51f},
    {2663.56f, -3464.43f, 262.66f, 5.20f},
};

enum Events
{
    EVENT_NONE,
    EVENT_BERSERK,
    EVENT_CURSE,
    EVENT_BLINK,
    EVENT_WARRIOR,
    EVENT_BALCONY,
    EVENT_WAVE,
    EVENT_GROUND,
};

class boss_noth : public CreatureScript
{
public:
    boss_noth() : CreatureScript("boss_noth") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new boss_nothAI (creature);
    }

    struct boss_nothAI : public BossAI
    {
        boss_nothAI(Creature* c) : BossAI(c, BOSS_NOTH) {}

        uint32 waveCount, balconyCount;

        void Reset()
        {
            me->SetReactState(REACT_AGGRESSIVE);
            me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            _Reset();
            SetImmuneToDeathGrip();
        }

        void EnterCombat(Unit* /*who*/)
        {
            _EnterCombat();
            DoScriptText(SAY_AGGRO, me);
            balconyCount = 0;
            EnterPhaseGround();
        }

        void EnterPhaseGround()
        {
            me->SetReactState(REACT_AGGRESSIVE);
            me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            DoZoneInCombat();
            if (me->getThreatManager().isThreatListEmpty())
                EnterEvadeMode();
            else
            {
                me->getThreatManager().resetAllAggro();
                events.ScheduleEvent(EVENT_BALCONY, 110000);
                events.ScheduleEvent(EVENT_CURSE, 10000+rand()%15000);
                events.ScheduleEvent(EVENT_WARRIOR, 30000);
                if (GetDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL)
                    events.ScheduleEvent(EVENT_BLINK, 20000 + rand()%20000);
            }
        }

        void KilledUnit(Unit* /*victim*/)
        {
            if (!(rand()%5))
                DoScriptText(SAY_SLAY, me);
        }

        void JustSummoned(Creature* summon)
        {
            summons.Summon(summon);
            summon->setActive(true);
            summon->AI()->DoZoneInCombat();
        }

        void JustDied(Unit* /*Killer*/)
        {
            _JustDied();
            DoScriptText(SAY_DEATH, me);
        }

        void SummonUndead(uint32 entry, uint32 num)
        {
            for (uint32 i = 0; i < num; ++i)
            {
                uint32 pos = RAID_MODE(RAND(2,3), rand()%MAX_SUMMON_POS);
                me->SummonCreature(entry, SummonPos[pos][0], SummonPos[pos][1], SummonPos[pos][2],
                    SummonPos[pos][3], TEMPSUMMON_CORPSE_DESPAWN, 60000);
            }
        }

        void UpdateAI(const uint32 diff)
        {
            if (!UpdateVictim() || !CheckInRoom())
                return;

            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch(eventId)
                {
                    case EVENT_CURSE:
                        if(!me->IsNonMeleeSpellCasted(false))
                        {
                            DoCastAOE(SPELL_CURSE_PLAGUEBRINGER);
                            events.ScheduleEvent(EVENT_CURSE, 50000 + rand()%10000);
                        }
                        return;
                    case EVENT_WARRIOR:
                        DoScriptText(SAY_SUMMON, me);
                        SummonUndead(MOB_WARRIOR, RAID_MODE(2, 3));
                        events.ScheduleEvent(EVENT_WARRIOR, 30000);
                        return;
                    case EVENT_BLINK:
                        if(!me->IsNonMeleeSpellCasted(false))
                        {
                            DoCastAOE(SPELL_CRIPPLE, true);
                            DoCastAOE(SPELL_BLINK);
                            DoResetThreat();
                            events.ScheduleEvent(EVENT_BLINK, 20000);
                        }
                        return;
                    case EVENT_BALCONY:
                        me->SetReactState(REACT_PASSIVE);
                        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                        me->AttackStop();
                        me->RemoveAllAuras();
                        me->NearTeleportTo(TELE_X, TELE_Y, TELE_Z, TELE_O);
                        me->getThreatManager().resetAllAggro();
                        events.Reset();
                        events.ScheduleEvent(EVENT_WAVE, 10000);
                        waveCount = 0;
                        return;
                    case EVENT_WAVE:
                        DoScriptText(SAY_SUMMON, me);
                        switch(balconyCount)
                        {
                            case 0: SummonUndead(MOB_CHAMPION, RAID_MODE(2, 4)); break;
                            case 1: SummonUndead(MOB_CHAMPION, RAID_MODE(1, 2));
                                    SummonUndead(MOB_GUARDIAN, RAID_MODE(1, 2)); break;
                            case 2: SummonUndead(MOB_GUARDIAN, RAID_MODE(2, 4)); break;
                            default:SummonUndead(MOB_CHAMPION, RAID_MODE(5, 10));
                                    SummonUndead(MOB_GUARDIAN, RAID_MODE(5, 10));break;
                        }
                        ++waveCount;
                        events.ScheduleEvent(waveCount < 2 ? EVENT_WAVE : EVENT_GROUND, 30000);
                        return;
                    case EVENT_GROUND:
                    {
                        ++balconyCount;
                        float x, y, z, o;
                        me->GetHomePosition(x, y, z, o);
                        me->NearTeleportTo(x, y, z, o);
                        events.ScheduleEvent(EVENT_BALCONY, 110000);
                        EnterPhaseGround();
                        return;
                    }
                }
            }

            if(balconyCount > 3)
            {
                if(!me->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE) && !me->HasAura(SPELL_BERSERK))
                    DoCast(me,SPELL_BERSERK,true);
            }

            if (me->HasReactState(REACT_AGGRESSIVE))
                DoMeleeAttackIfReady();
        }
    };

};

enum eSpellsTrash
{
    // 16984
    SPELL_CLEAVE                = 15496,
    // 16983
    SPELL_MORTAL_STRIKE         = 32736,
    SPELL_SHADOW_SHOCK          = 30138,
    SPELL_SHADOW_SHOCK_H        = 54889,
    // 16981
    SPELL_ARCANE_EXPLOSION      = 54890,
    SPELL_ARCANE_EXPLOSION_H    = 54891,
    SPELL_BLINK_1               = 29208,
    SPELL_BLINK_2               = 29209,
    SPELL_BLINK_3               = 29210,
    SPELL_BLINK_4               = 29211,
};

class mob_naxxramas_noth_trash : public CreatureScript
{
public:
    mob_naxxramas_noth_trash() : CreatureScript("mob_naxxramas_noth_trash") { }

    CreatureAI* GetAI(Creature* pCreature) const
    {
        switch(pCreature->GetEntry())
        {
            case 16984: return new mob_plagued_warriorAI (pCreature);
            case 16983: return new mob_plagued_championAI (pCreature);
            case 16981: return new mob_plagued_guardianAI (pCreature);
            default: return NULL;
        }
    }

    struct mob_plagued_warriorAI : ScriptedAI 
    {
        mob_plagued_warriorAI(Creature *c) : ScriptedAI(c){}

        uint32 uiCleave_Timer;

        void Reset()
        {
            uiCleave_Timer = urand(5000,10000);
        }

        void EnterCombat(Unit* /*who*/) {}
        void UpdateAI(const uint32 diff)
        {
            if (!UpdateVictim() )
                return;

            if(uiCleave_Timer <= diff)
            {
                DoCast(me->getVictim(),SPELL_CLEAVE);
                uiCleave_Timer = urand(5000,10000);
            }else uiCleave_Timer -= diff;

            DoMeleeAttackIfReady();
        }
    };

    struct mob_plagued_championAI : ScriptedAI 
    {
        mob_plagued_championAI(Creature *c) : ScriptedAI(c){}

        uint32 uiMortalStrike_Timer;
        uint32 uiShadowShock_Timer;

        void Reset()
        {
            uiMortalStrike_Timer = urand(5000,10000);
            uiShadowShock_Timer = urand(10000,15000);
        }

        void EnterCombat(Unit* /*who*/) {}
        void UpdateAI(const uint32 diff)
        {
            if (!UpdateVictim() )
                return;

            if(uiMortalStrike_Timer <= diff)
            {
                DoCast(me->getVictim(),SPELL_CLEAVE);
                uiMortalStrike_Timer = urand(5000,10000);
            }else uiMortalStrike_Timer -= diff;

            if(uiShadowShock_Timer <= diff)
            {
                DoCastAOE(RAID_MODE(SPELL_SHADOW_SHOCK,SPELL_SHADOW_SHOCK_H));
                uiShadowShock_Timer = urand(10000,15000);
            }else uiShadowShock_Timer -= diff;

            DoMeleeAttackIfReady();
        }
    };

    struct mob_plagued_guardianAI : ScriptedAI 
    {
        mob_plagued_guardianAI(Creature *c) : ScriptedAI(c){}

        uint32 uiExplosion_Timer;
        uint32 uiBlink_Timer;

        void Reset()
        {
            uiExplosion_Timer = urand(7000,12000);
            uiBlink_Timer = urand(10000,15000);
        }

        void EnterCombat(Unit* /*who*/) {}
        void UpdateAI(const uint32 diff)
        {
            if (!UpdateVictim() )
                return;

            if(uiBlink_Timer <= diff)
            {
                if(Unit* target = SelectTarget(SELECT_TARGET_RANDOM,1,40,true))
                    DoCast(target,RAND(SPELL_BLINK_1,SPELL_BLINK_2,SPELL_BLINK_3,SPELL_BLINK_4));
                uiBlink_Timer = urand(10000,20000);
            }else uiBlink_Timer -= diff;

            if(uiExplosion_Timer <= diff)
            {
                DoCastAOE(RAID_MODE(SPELL_ARCANE_EXPLOSION,SPELL_ARCANE_EXPLOSION_H));
                uiExplosion_Timer = urand(7000,12000);
            }else uiExplosion_Timer -= diff;

            DoMeleeAttackIfReady();
        }
    };
};

void AddSC_boss_noth()
{
    new boss_noth();
    new mob_naxxramas_noth_trash();
}

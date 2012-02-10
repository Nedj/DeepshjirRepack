/*
 * Copyright (C) 2005 - 2011 MaNGOS <http://www.getmangos.org/>
 *
 * Copyright (C) 2008 - 2011 TrinityCore <http://www.trinitycore.org/>
 *
 * Copyright (C) 2011 - 2012 ArkCORE <http://www.arkania.net/>
 *
 * Copyright (C) 2012 DeepshjirCataclysm Repack
 * By Naios
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
#include "blackwing_descent.h"

enum Events
{
    EVENT_DOUBLE_ATTACK = 1,
    EVENT_CAUSTIC_SLIME,
    EVENT_MASSACRE,

};

enum Actions
{
    ACTION_BILE_O_TRON_EVENT_START      = 1,
    ACTION_BILE_O_TRON_SYSTEM_FAILURE,
    ACTION_RESET,
};

enum Spells
{
    // Chimaeron
    SPELL_MASSACRE                      = 82848,

    // Bile O Tron
    SPELL_FINKLES_MIXTURE               = 82705,
    SPELL_FINKLES_MIXTURE_VISUAL        = 91106,
    SPELL_SYSTEM_FALURE                 = 88853,
    SPELL_REROUTE_POWER                 = 88861,
};

Position const BilePositions[6] =
{
    {-135.795151f, 15.569847f, 73.165909f, 4.646072f},
    {-129.176636f, -10.488489f, 73.079071f, 5.631739f},
    {-106.186249f, -18.533386f, 72.798332f, 1.555510f},
    {-77.951973f, 0.702321f, 73.093552f, 1.509125f},
    {-77.466125f, 31.038124f, 73.177673f, 4.489712f},
    {-120.426445f, 34.491863f, 72.057610f, 4.116642f},
};

class boss_chimaeron : public CreatureScript
{
public:
    boss_chimaeron() : CreatureScript("boss_chimaeron") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new boss_chimaeronAI (creature);
    }

    struct boss_chimaeronAI: public BossAI
    {
		boss_chimaeronAI(Creature* creature) : BossAI(creature, DATA_CHIMAERON)
        {
            instance = creature->GetInstanceScript();
        }

        InstanceScript* instance;
		EventMap events;
		
        void Reset()
		{
			events.Reset();
            me->SetReactState(REACT_PASSIVE);

            if(Creature* finkle_einhorn = ObjectAccessor::GetCreature(*me,instance->GetData64(NPC_FINKLE_EINHORN)))
                finkle_einhorn->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);

            if(Creature* bile_o_tron = ObjectAccessor::GetCreature(*me,instance->GetData64(NPC_BILE_O_TRON)))
                bile_o_tron->AI()->DoAction(ACTION_RESET);

            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
		}

        void EnterCombat(Unit* /*who*/)
		{		
			events.ScheduleEvent(EVENT_MASSACRE, urand(10000,12000));
		}

        void UpdateAI(const uint32 diff)
		{
			if (!UpdateVictim() || me->HasUnitState(UNIT_STAT_CASTING))
				return;
			
			events.Update(diff);

			while (uint32 eventId = events.ExecuteEvent())
			{
				switch (eventId)
				{

				case EVENT_MASSACRE:
					DoCastVictim(SPELL_MASSACRE);
                    if(Creature* bile_o_tron = ObjectAccessor::GetCreature(*me,instance->GetData64(NPC_BILE_O_TRON)))
                        bile_o_tron->AI()->DoAction(ACTION_BILE_O_TRON_SYSTEM_FAILURE);

					events.ScheduleEvent(EVENT_MASSACRE, urand(40000,45000));
					break;
				
				default:
					break;
				}
			}		

            DoMeleeAttackIfReady();
        }

        void DamageTaken(Unit* who, uint32& damage)
		{
            if(me->HasReactState(REACT_PASSIVE))
            {
                me->SetReactState(REACT_AGGRESSIVE);
                DoZoneInCombat(me);
            }
        }
    };
};

class mob_finkle_einhorn : public CreatureScript
{
public:
	mob_finkle_einhorn() : CreatureScript("mob_finkle_einhorn") { }

	bool OnGossipHello(Player* pPlayer, Creature* creature)
	{

        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,"We need Bile-O-Trons Help!",GOSSIP_SENDER_MAIN ,GOSSIP_ACTION_INFO_DEF+1);
		pPlayer->SEND_GOSSIP_MENU(1,creature->GetGUID());

		return true;
	}

	bool OnGossipSelect(Player* pPlayer, Creature* creature, uint32 uiSender, uint32 uiAction)
	{
		pPlayer->PlayerTalkClass->ClearMenus();

		pPlayer->CLOSE_GOSSIP_MENU();

        if(InstanceScript* instance = creature->GetInstanceScript())
        {
            if(Creature* bile_o_tron = ObjectAccessor::GetCreature(*creature,instance->GetData64(NPC_BILE_O_TRON)))
            {
                bile_o_tron->AI()->DoAction(ACTION_BILE_O_TRON_EVENT_START);
                creature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
            }

            if(Creature* chimaeron = ObjectAccessor::GetCreature(*creature,instance->GetData64(BOSS_CHIMAERON)))
                chimaeron->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        }
		return true;
	}
};

class mob_bile_o_tron : public CreatureScript
{
public:
    mob_bile_o_tron() : CreatureScript("mob_bile_o_tron") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new mob_bile_o_tronAI (creature);
    }

    struct mob_bile_o_tronAI : public ScriptedAI
    {
        mob_bile_o_tronAI(Creature* creature) : ScriptedAI(creature), Waypoint(7), uiSystemFailureTimer(0)
        {
            instance = creature->GetInstanceScript();
            creature->AddUnitMovementFlag(MOVEMENTFLAG_WALKING);
        }

        InstanceScript* instance;
        uint8 Waypoint;
        uint32 uiSystemFailureTimer;
        		
        void UpdateAI(const uint32 diff)
		{
            if(uiSystemFailureTimer == 0)
                return;

            if(uiSystemFailureTimer <= diff)
            {
                me->RemoveAura(SPELL_SYSTEM_FALURE);
                me->GetMotionMaster()->MovePoint(1,BilePositions[Waypoint]);
                uiSystemFailureTimer = 0;

                if(instance)
                    instance->DoCastSpellOnPlayers(SPELL_FINKLES_MIXTURE);
            }
            else uiSystemFailureTimer -= diff;
        }

        void DoAction(const int32 action)
        {
            switch(action)
            {
            
            case ACTION_BILE_O_TRON_EVENT_START:
                DoCast(me,SPELL_FINKLES_MIXTURE_VISUAL,true);
                if(instance)
                    instance->DoCastSpellOnPlayers(SPELL_FINKLES_MIXTURE);
                Waypoint = 8;
                me->GetMotionMaster()->MovePoint(1,BilePositions[0]);
                break;

            case ACTION_BILE_O_TRON_SYSTEM_FAILURE:
                me->MonsterYell("System failure! brzzz",0,0);
                me->RemoveAllAuras();
                DoCast(me,SPELL_REROUTE_POWER, true);
                DoCast(me,SPELL_SYSTEM_FALURE, true);
                
                uiSystemFailureTimer = 26000;

                if(instance)
                    instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_FINKLES_MIXTURE);

                break;

            case ACTION_RESET:
                me->RemoveAllAuras();
                me->GetMotionMaster()->MoveTargetedHome();
                Waypoint = 7;
                uiSystemFailureTimer = 0;
                break;
            }
        }

        void MovementInform(uint32 type, uint32 id)
		{
			if (type != POINT_MOTION_TYPE || Waypoint == 7)
				return;

            if(Waypoint <= 5)
                Waypoint++;
            else
                Waypoint = 0;

            me->GetMotionMaster()->MovePoint(1,BilePositions[Waypoint]);               
        }
    };
};

void AddSC_boss_chimaeron()
{
    new boss_chimaeron();
    new mob_finkle_einhorn();
    new mob_bile_o_tron();
}
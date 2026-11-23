#include "PAGameplayTags.h"

#include "GameplayTagsManager.h"

namespace PATags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Race_Human, "Enemy.Race.Human", "종족: 사람");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Race_Robot, "Enemy.Race.Robot", "종족: 로봇 (갑옷, EMP 기절 대상)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Rank_Normal, "Enemy.Rank.Normal", "특수 객체: 일반");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Rank_Elite, "Enemy.Rank.Elite", "특수 객체: 엘리트");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Rank_Boss, "Enemy.Rank.Boss", "특수 객체: 보스");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage, "SetByCaller.Damage", "탄종 피해: 기본 피해량");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Mult_Health, "SetByCaller.Mult.Health", "탄종 피해: 체력 배율");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Mult_Armor, "SetByCaller.Mult.Armor", "탄종 피해: 갑옷 배율");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Mult_Shield, "SetByCaller.Mult.Shield", "탄종 피해: 실드 배율");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Weakpoint, "SetByCaller.Weakpoint", "탄종 피해: 약점 배율(1 = 일반 명중)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Duration, "SetByCaller.Duration", "지속 시간(기절·쿨다운)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Stat_DamageMult, "SetByCaller.Stat.DamageMult", "모듈 패시브: 주는 피해 배율 가산");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Stat_MeleeDamageMult, "SetByCaller.Stat.MeleeDamageMult", "모듈 패시브: 근접 피해 배율 가산");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Stat_MeleeArmorBonus, "SetByCaller.Stat.MeleeArmorBonus", "모듈 패시브: 근접의 갑옷 배율 가산");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Stat_MeleeKnockback, "SetByCaller.Stat.MeleeKnockback", "모듈 패시브: 근접 넉백 세기");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Stat_WeakpointBonus, "SetByCaller.Stat.WeakpointBonus", "모듈 패시브: 약점 배율 가산");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Stat_ArmorPierceBonus, "SetByCaller.Stat.ArmorPierceBonus", "모듈 패시브: 갑옷 상대 배율 가산");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Stat_CooldownMult, "SetByCaller.Stat.CooldownMult", "모듈 패시브: 스킬 쿨다운 배율 가산(음수 = 감소)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Stat_ReloadMult, "SetByCaller.Stat.ReloadMult", "모듈 패시브: 재장전 시간 배율 가산(음수 = 빨라짐)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Stat_MoveSpeedMult, "SetByCaller.Stat.MoveSpeedMult", "모듈 패시브: 이동 속도 배율 가산");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Stat_ExtraJumps, "SetByCaller.Stat.ExtraJumps", "모듈 패시브: 추가 점프 횟수");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Stat_MaxShield, "SetByCaller.Stat.MaxShield", "모듈 패시브: 최대 실드 가산");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Melee, "Ability.Melee", "근접 공격(V)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Module, "Ability.Module", "모듈이 준 스킬");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Module, "Cooldown.Module", "모듈 스킬 쿨다운 (하위 태그는 모듈 ID)");

	static FGameplayTag RequestOptional(const TCHAR* Name)
	{
		return FGameplayTag::RequestGameplayTag(FName(Name), /*ErrorIfNotFound*/ false);
	}

	FGameplayTag GetStunnedTag() { return RequestOptional(TEXT("State.Stunned")); }
	FGameplayTag GetDeadTag() { return RequestOptional(TEXT("State.Dead")); }
	FGameplayTag GetImmuneStunTag() { return RequestOptional(TEXT("Immune.Stun")); }
	FGameplayTag GetLegacyBossTag() { return RequestOptional(TEXT("Enemy.Boss")); }

	FGameplayTag StatNameToTag(FName StatName)
	{
		static const TMap<FName, FGameplayTag> Map = []
		{
			TMap<FName, FGameplayTag> M;
			M.Add(TEXT("DamageMult"), SetByCaller_Stat_DamageMult);
			M.Add(TEXT("MeleeDamageMult"), SetByCaller_Stat_MeleeDamageMult);
			M.Add(TEXT("MeleeArmorBonus"), SetByCaller_Stat_MeleeArmorBonus);
			M.Add(TEXT("MeleeKnockback"), SetByCaller_Stat_MeleeKnockback);
			M.Add(TEXT("WeakpointBonus"), SetByCaller_Stat_WeakpointBonus);
			M.Add(TEXT("ArmorPierceBonus"), SetByCaller_Stat_ArmorPierceBonus);
			M.Add(TEXT("CooldownMult"), SetByCaller_Stat_CooldownMult);
			M.Add(TEXT("ReloadMult"), SetByCaller_Stat_ReloadMult);
			M.Add(TEXT("MoveSpeedMult"), SetByCaller_Stat_MoveSpeedMult);
			M.Add(TEXT("ExtraJumps"), SetByCaller_Stat_ExtraJumps);
			M.Add(TEXT("MaxShield"), SetByCaller_Stat_MaxShield);
			return M;
		}();
		const FGameplayTag* Found = Map.Find(StatName);
		return Found ? *Found : FGameplayTag();
	}
}

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * 코드에서 쓰는 게임플레이 태그 (네이티브 태그).
 * Unity로 치면 문자열 상수를 모아 둔 static 클래스 — 오타가 나면 컴파일 에러가 난다.
 *
 * DefaultGameplayTags.ini에 이미 있던 태그(State.Stunned, Immune.Stun, Enemy.Boss 등)는
 * 여기서 다시 정의하지 않고 PATags::Get*()로 이름을 찾아 쓴다(정의가 두 곳에 생기지 않게).
 */
namespace PATags
{
	// 적 분류: 종족
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Race_Human);
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Race_Robot);
	// 적 분류: 특수 객체(등급)
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Rank_Normal);
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Rank_Elite);
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Rank_Boss);

	// 피해 계산에 넘기는 SetByCaller 값
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Mult_Health);
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Mult_Armor);
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Mult_Shield);
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Weakpoint);
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Duration);

	// 모듈 패시브 스탯 (SetByCaller.Stat.<속성 이름>)
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Stat_DamageMult);
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Stat_MeleeDamageMult);
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Stat_MeleeArmorBonus);
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Stat_MeleeKnockback);
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Stat_WeakpointBonus);
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Stat_ArmorPierceBonus);
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Stat_CooldownMult);
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Stat_ReloadMult);
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Stat_MoveSpeedMult);
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Stat_ExtraJumps);
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Stat_MaxShield);

	// 스킬
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Melee);
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Module);
	UNREALPROJECT_A_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Module);

	/** ini에 이미 있는 태그. 없으면 빈 태그를 돌려준다(에러 없이). */
	UNREALPROJECT_A_API FGameplayTag GetStunnedTag();      // State.Stunned
	UNREALPROJECT_A_API FGameplayTag GetDeadTag();         // State.Dead
	UNREALPROJECT_A_API FGameplayTag GetImmuneStunTag();   // Immune.Stun
	UNREALPROJECT_A_API FGameplayTag GetLegacyBossTag();   // Enemy.Boss (예전 보스 태그, Rank.Boss와 같은 뜻으로 취급)

	/** 패시브 스탯 이름(FName) → SetByCaller 태그. 모르는 이름이면 빈 태그. */
	UNREALPROJECT_A_API FGameplayTag StatNameToTag(FName StatName);
}

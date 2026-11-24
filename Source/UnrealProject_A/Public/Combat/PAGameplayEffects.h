#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "PAGameplayEffects.generated.h"

/**
 * 코드로 정의한 GameplayEffect들. 에셋(.uasset)이 없어도 동작하게 하려고 C++로 만들었다.
 * BP에서 같은 일을 하는 GE를 만들고 싶으면 이 클래스를 부모로 BP를 만들면 된다.
 */

/** 탄종 피해: 즉시 적용, 계산은 UPADamageExecution. */
UCLASS()
class UNREALPROJECT_A_API UPAGE_LayeredDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPAGE_LayeredDamage();
};

/**
 * 지속 시간 = SetByCaller.Duration. 붙는 태그는 스펙의 DynamicGrantedTags로 넣는다.
 * 기절(State.Stunned)과 모듈 쿨다운(Cooldown.Module.*)이 같이 쓴다.
 */
UCLASS()
class UNREALPROJECT_A_API UPAGE_TimedTag : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPAGE_TimedTag();
};

/**
 * 모듈 패시브: 무한 지속. 모든 보정 스탯에 SetByCaller.Stat.* 값을 더한다.
 * 모듈 하나당 스펙 하나를 붙이고, 모듈을 빼면 그 스펙만 지운다.
 */
UCLASS()
class UNREALPROJECT_A_API UPAGE_ModulePassive : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPAGE_ModulePassive();

	/** 이 GE가 다루는 스탯 이름 목록 (스펙을 만들 때 전부 0으로 먼저 채운다 — 빠진 값 경고 방지). */
	static const TArray<FName>& GetStatNames();
};

#include "PAAbilitySystemComponent.h"

#include "PAAttributeSet.h"
#include "PACombatAttributeSet.h"

UPAAbilitySystemComponent::UPAAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);
	StartingAttributeSets.Add(UPAAttributeSet::StaticClass());
}

void UPAAbilitySystemComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// 스탯 칸은 서버에서만 만든다. 클라이언트는 복제로 받는다.
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	for (const TSubclassOf<UAttributeSet>& SetClass : StartingAttributeSets)
	{
		if (SetClass)
		{
			GetOrCreateAttributeSubobject(SetClass);
		}
	}

	// 공격 보정 스탯은 항상 만든다. BP 컴포넌트에 StartingAttributeSets가 따로 저장돼 있어도
	// 빠지지 않게 하려는 것(배열이 BP에 직렬화돼 있으면 생성자 기본값이 무시된다).
	GetOrCreateAttributeSubobject(UPACombatAttributeSet::StaticClass());
}

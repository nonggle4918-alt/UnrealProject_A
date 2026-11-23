#include "PAAbilitySystemComponent.h"

#include "PAAttributeSet.h"

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
}

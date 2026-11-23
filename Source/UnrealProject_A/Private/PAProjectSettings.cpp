#include "PAProjectSettings.h"

#include "InputCoreTypes.h"
#include "Skills/PAModuleAbilities.h"

namespace
{
	FPAAmmoProfile MakeAmmo(const TCHAR* Id, const TCHAR* Name, float H, float A, float S, float Stun = 0.f, int32 Mag = 0)
	{
		FPAAmmoProfile P;
		P.Id = FName(Id);
		P.DisplayName = FText::FromString(Name);
		P.HealthMult = H;
		P.ArmorMult = A;
		P.ShieldMult = S;
		P.RobotStunSeconds = Stun;
		P.MagazineOverride = Mag;
		return P;
	}

	FPAModuleDefinition MakeModule(const TCHAR* Id, const TCHAR* Name, const TCHAR* Desc, EPAModuleCategory Cat,
		int32 Compute, int32 Price, bool bStarter)
	{
		FPAModuleDefinition M;
		M.Id = FName(Id);
		M.DisplayName = FText::FromString(Name);
		M.Description = FText::FromString(Desc);
		M.Category = Cat;
		M.ComputeCost = Compute;
		M.Price = Price;
		M.bStarter = bStarter;
		return M;
	}

	void AddStat(FPAModuleDefinition& M, const TCHAR* Stat, float Value)
	{
		FPAModuleStatMod Mod;
		Mod.Stat = FName(Stat);
		Mod.Value = Value;
		M.PassiveStats.Add(Mod);
	}

	void SetActive(FPAModuleDefinition& M, TSubclassOf<UGameplayAbility> Ability, FKey Key, float Cooldown,
		float P1 = 0.f, float P2 = 0.f, float P3 = 0.f)
	{
		M.ActiveAbility = TSoftClassPtr<UGameplayAbility>(Ability.Get());
		M.ActivationKey = Key;
		M.Cooldown = Cooldown;
		M.Param1 = P1;
		M.Param2 = P2;
		M.Param3 = P3;
	}

	FPARankModifier MakeRank(float HealthMult, float ArmorMult, float BonusArmor, float BonusShield,
		float StunMult, bool bImmune, int32 Credits, int32 Xp)
	{
		FPARankModifier R;
		R.HealthMult = HealthMult;
		R.ArmorMult = ArmorMult;
		R.BonusArmor = BonusArmor;
		R.BonusShield = BonusShield;
		R.StunDurationMult = StunMult;
		R.bStunImmune = bImmune;
		R.CreditReward = Credits;
		R.GrowthXpReward = Xp;
		return R;
	}

	FPAGrowthForm MakeForm(const TCHAR* Id, const TCHAR* Name, const TCHAR* Desc, const TCHAR* Parent, int32 Level,
		const TCHAR* Ammo, float Dmg, float Rate, int32 Mag)
	{
		FPAGrowthForm F;
		F.Id = FName(Id);
		F.DisplayName = FText::FromString(Name);
		F.Description = FText::FromString(Desc);
		F.ParentId = Parent ? FName(Parent) : NAME_None;
		F.RequiredLevel = Level;
		F.AmmoId = FName(Ammo);
		F.DamageMult = Dmg;
		F.FireRateMult = Rate;
		F.MagazineBonus = Mag;
		return F;
	}
}

UPAProjectSettings::UPAProjectSettings()
{
	// ---------- 탄종 [제안] ----------
	// 배율은 3단계로 단순화: 유효 1.5 / 보통 1.0 / 저항 0.7 (특수탄만 2.0).
	// 넘친 피해는 원래 피해량 그대로 다음 층으로 넘어간다(환산 없음 → 플레이어가 읽기 쉽게).
	AmmoProfiles = {
		MakeAmmo(TEXT("Normal"), TEXT("일반탄"), 1.f, 1.f, 1.f),
		MakeAmmo(TEXT("5mm"), TEXT("5mm 고속탄"), 1.0f, 0.7f, 1.5f),
		MakeAmmo(TEXT("7mm"), TEXT("7mm 소총탄"), 1.5f, 1.0f, 0.7f),
		MakeAmmo(TEXT("9mm"), TEXT("9mm 권총탄"), 1.0f, 0.7f, 1.0f),
		MakeAmmo(TEXT("AP"), TEXT("철갑탄"), 0.7f, 1.5f, 0.7f),
		MakeAmmo(TEXT("ShieldBreaker"), TEXT("실드 파괴탄"), 0.7f, 1.0f, 2.0f),
		MakeAmmo(TEXT("EMP"), TEXT("EMP탄"), 0.7f, 1.0f, 1.5f, /*Stun*/ 1.5f, /*Mag*/ 1),
		MakeAmmo(TEXT("Melee"), TEXT("근접"), 1.0f, 0.7f, 1.0f),
		MakeAmmo(TEXT("Explosive"), TEXT("폭발"), 1.0f, 1.5f, 0.7f),
	};

	// ---------- 적 [제안] ----------
	Human.MaxHealth = 100.f;
	Human.MaxArmor = 0.f;
	Human.MaxShield = 0.f;

	Robot.MaxHealth = 80.f;
	Robot.MaxArmor = 50.f;
	Robot.MaxShield = 0.f;

	NormalRank = MakeRank(1.f, 1.f, 0.f, 0.f, 1.f, false, 10, 10);
	EliteRank = MakeRank(2.f, 1.5f, 0.f, 60.f, 0.5f, false, 40, 40);
	BossRank = MakeRank(1.f, 1.f, 200.f, 300.f, 0.f, true, 150, 150);

	// ---------- 모듈 [제안] ----------
	// 시작 연산력 12, 기본 지급(블레이드 2 + 스프링 2 + 침투 3) = 7 → 처음부터 5가 남는다.
	{
		FPAModuleDefinition M = MakeModule(TEXT("Arm.Blade"), TEXT("열절단 블레이드"),
			TEXT("V 근접 피해 ×1.8, 근접이 갑옷에 1.5배."), EPAModuleCategory::Arm, 2, 0, true);
		AddStat(M, TEXT("MeleeDamageMult"), 0.8f);
		AddStat(M, TEXT("MeleeArmorBonus"), 0.8f);
		Modules.Add(M);
	}
	{
		FPAModuleDefinition M = MakeModule(TEXT("Arm.Hydraulic"), TEXT("유압 증폭 팔"),
			TEXT("근접 피해 ×1.4 + 넉백, 재장전 시간 ×0.85."), EPAModuleCategory::Arm, 3, 300, false);
		AddStat(M, TEXT("MeleeDamageMult"), 0.4f);
		AddStat(M, TEXT("MeleeKnockback"), 900.f);
		AddStat(M, TEXT("ReloadMult"), -0.15f);
		Modules.Add(M);
	}
	{
		FPAModuleDefinition M = MakeModule(TEXT("Arm.Launcher"), TEXT("마이크로 미사일 포드"),
			TEXT("[F] 폭발 탄 3발 연사. 갑옷에 강함."), EPAModuleCategory::Arm, 4, 500, false);
		SetActive(M, UPAGA_MicroMissile::StaticClass(), EKeys::F, 10.f, /*피해*/ 45.f, /*발 수*/ 3.f, /*반경*/ 250.f);
		Modules.Add(M);
	}
	{
		FPAModuleDefinition M = MakeModule(TEXT("Arm.Stabilizer"), TEXT("반동 안정 팔"),
			TEXT("약점 배율 +0.5 (2.0 → 2.5)."), EPAModuleCategory::Arm, 2, 300, false);
		AddStat(M, TEXT("WeakpointBonus"), 0.5f);
		Modules.Add(M);
	}
	{
		FPAModuleDefinition M = MakeModule(TEXT("Leg.Spring"), TEXT("스프링 힘줄"),
			TEXT("2단 점프."), EPAModuleCategory::Leg, 2, 0, true);
		AddStat(M, TEXT("ExtraJumps"), 1.f);
		Modules.Add(M);
	}
	{
		FPAModuleDefinition M = MakeModule(TEXT("Leg.Thruster"), TEXT("분사 발목"),
			TEXT("[C] 이동 방향으로 대시(공중 가능)."), EPAModuleCategory::Leg, 3, 375, false);
		SetActive(M, UPAGA_ThrusterDash::StaticClass(), EKeys::C, 3.f, /*속도*/ 1800.f);
		Modules.Add(M);
	}
	{
		FPAModuleDefinition M = MakeModule(TEXT("Leg.Servo"), TEXT("서보 근섬유"),
			TEXT("이동 속도 +15%."), EPAModuleCategory::Leg, 2, 250, false);
		AddStat(M, TEXT("MoveSpeedMult"), 0.15f);
		Modules.Add(M);
	}
	{
		FPAModuleDefinition M = MakeModule(TEXT("Brain.Accelerator"), TEXT("신경 가속기"),
			TEXT("[Z] 4초간 세계가 절반 속도로 흐른다(시전자 제외)."), EPAModuleCategory::Brain, 5, 625, false);
		SetActive(M, UPAGA_NeuralAccelerator::StaticClass(), EKeys::Z, 35.f, /*세계 속도*/ 0.5f, /*지속*/ 4.f);
		Modules.Add(M);
	}
	{
		FPAModuleDefinition M = MakeModule(TEXT("Brain.Breach"), TEXT("침투 데크"),
			TEXT("[X] 조준한 적 해킹: 로봇 기절, 사람은 회로 피해."), EPAModuleCategory::Brain, 3, 0, true);
		SetActive(M, UPAGA_Breach::StaticClass(), EKeys::X, 12.f, /*사거리*/ 3000.f, /*기절*/ 2.5f, /*사람 피해*/ 40.f);
		Modules.Add(M);
	}
	{
		FPAModuleDefinition M = MakeModule(TEXT("Brain.Overclock"), TEXT("오버클럭 칩"),
			TEXT("모듈 스킬 쿨다운 ×0.7."), EPAModuleCategory::Brain, 3, 450, false);
		AddStat(M, TEXT("CooldownMult"), -0.3f);
		Modules.Add(M);
	}
	{
		FPAModuleDefinition M = MakeModule(TEXT("Brain.Ballistics"), TEXT("탄도 연산 칩"),
			TEXT("모든 공격의 갑옷 배율 +0.3."), EPAModuleCategory::Brain, 3, 375, false);
		AddStat(M, TEXT("ArmorPierceBonus"), 0.3f);
		Modules.Add(M);
	}
	{
		FPAModuleDefinition M = MakeModule(TEXT("Brain.Barrier"), TEXT("개인 실드 생성기"),
			TEXT("실드 40. 피격이 멈추면 재생."), EPAModuleCategory::Brain, 4, 500, false);
		AddStat(M, TEXT("MaxShield"), 40.f);
		Modules.Add(M);
	}

	// 총기 해금 [제안]: 회의의 "개조에 따른 총기 해금"을 모듈 보유 조건으로 연결.
	{
		FPAWeaponUnlockRule R;
		R.WeaponId = TEXT("EMPPistol");
		R.DisplayName = FText::FromString(TEXT("EMP 권총"));
		R.RequiredModuleId = TEXT("Brain.Breach");
		WeaponUnlocks.Add(R);
	}
	{
		FPAWeaponUnlockRule R;
		R.WeaponId = TEXT("APRifle");
		R.DisplayName = FText::FromString(TEXT("철갑 소총"));
		R.RequiredModuleId = TEXT("Brain.Ballistics");
		WeaponUnlocks.Add(R);
	}

	MeleeKey = EKeys::V;

	// ---------- 성장형 총 [제안] ----------
	// 프로토타입 범위: 시작 형태 1 + 1차 진화 3갈래(Lv4) + 2차 진화 갈래당 1개(Lv7).
	// Lv7은 킬만으로는 한 스테이지에 닿지 않고(Lv5~6) 크레드 '데이터 공급'을 크게 해야 닿는다
	// → 보스 직전 "수입을 걸고 2차 진화할지, 남길지" 선택 (기획_경제시뮬레이션_가안.md).
	// 2차 진화의 화력 상승(초당 피해 기준 약 +15~40%)은 증강 몇 개 분량 — 밸런스 기획 Ver02의 TTK 예산과 같이 볼 것.
	GrowthForms = {
		MakeForm(TEXT("Gun.Seed"), TEXT("각성 전"), TEXT("아직 잠든 총. 9mm 권총형."), nullptr, 1, TEXT("9mm"), 1.f, 1.f, 0),
		MakeForm(TEXT("Gun.Swarm"), TEXT("군집형"), TEXT("기관총형. 5mm 연사로 실드를 빠르게 벗긴다."), TEXT("Gun.Seed"), 4, TEXT("5mm"), 0.8f, 2.0f, 20),
		MakeForm(TEXT("Gun.Scatter"), TEXT("산탄형"), TEXT("샷건형. 7mm로 사람을 가까이서 제압."), TEXT("Gun.Seed"), 4, TEXT("7mm"), 1.0f, 0.6f, 0),
		MakeForm(TEXT("Gun.Lance"), TEXT("관통형"), TEXT("소총형. 철갑탄으로 로봇 장갑을 뚫는다."), TEXT("Gun.Seed"), 4, TEXT("AP"), 1.6f, 0.7f, 0),
		// 2차: 각 갈래의 장점을 키우거나 약점을 메운다.
		MakeForm(TEXT("Gun.Hive"), TEXT("벌집형"), TEXT("군집형의 끝. 탄창이 커지고 연사가 더 빨라진다. 실드를 녹인다."), TEXT("Gun.Swarm"), 7, TEXT("5mm"), 0.85f, 2.6f, 40),
		MakeForm(TEXT("Gun.Breaker"), TEXT("파쇄형"), TEXT("산탄형의 끝. 폭발 슬러그로 근거리 로봇 장갑까지 부순다."), TEXT("Gun.Scatter"), 7, TEXT("Explosive"), 1.3f, 0.6f, 0),
		MakeForm(TEXT("Gun.Rail"), TEXT("레일형"), TEXT("관통형의 끝. 느리지만 한 발이 무겁다. 약점 저격용."), TEXT("Gun.Lance"), 7, TEXT("AP"), 2.6f, 0.5f, 0),
	};
	GrowthXpPerLevel = { 60, 90, 120, 150, 180, 210, 240, 270, 300 }; // 최대 레벨 10

	RunMap = FSoftObjectPath(TEXT("/Game/Level/MainLevel_Random.MainLevel_Random"));
	LobbyMap = FSoftObjectPath(TEXT("/Game/Level/LV_Lobby.LV_Lobby"));
}

const FPAAmmoProfile* UPAProjectSettings::FindAmmo(FName AmmoId) const
{
	return AmmoProfiles.FindByPredicate([AmmoId](const FPAAmmoProfile& P) { return P.Id == AmmoId; });
}

const FPAModuleDefinition* UPAProjectSettings::FindModule(FName ModuleId) const
{
	return Modules.FindByPredicate([ModuleId](const FPAModuleDefinition& M) { return M.Id == ModuleId; });
}

const FPAGrowthForm* UPAProjectSettings::FindGrowthForm(FName FormId) const
{
	return GrowthForms.FindByPredicate([FormId](const FPAGrowthForm& F) { return F.Id == FormId; });
}

const FPARankModifier& UPAProjectSettings::GetRank(EPAEnemyRank Rank) const
{
	switch (Rank)
	{
	case EPAEnemyRank::Elite: return EliteRank;
	case EPAEnemyRank::Boss: return BossRank;
	default: return NormalRank;
	}
}

const FPARaceDefaults& UPAProjectSettings::GetRace(EPAEnemyRace Race) const
{
	return Race == EPAEnemyRace::Robot ? Robot : Human;
}

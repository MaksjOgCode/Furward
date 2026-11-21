// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает боевой ввод, камеру и команды, которые контроллер передаёт сессии.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CombatPlayerController.generated.h"



class ACombatArena;
class ACombatEnemy;
class UCombatRootWidget;
class UCombatSession;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
enum class ECombatClickResult : uint8;

/** @brief Сигнал о завершении боя после записи актуальной ревизии профиля. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatExitReady);



/**
 * @brief Обрабатывает ввод локального игрока на L_Combat.
 *
 * Unreal создаёт BP_Combat_PlayerController по настройкам GameMode. Локальный контроллер создаёт корневой HUD.
 * Контроллер существует в игровом мире до выхода с карты и использует боевой контекст локального игрока.
 */
UCLASS()
class ACombatPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	/**
	 * @brief Переключает вид игрока на камеру зарегистрированной арены.
	 *
	 * Сессия вызывает метод после появления окружения. При недействительной арене или неактивной камере контроллер пишет ошибку.
	 * @param[in] Arena Арена, найденная в подгруженном уровне.
	 * @see UCombatSession::HandleEnvironmentShown
	 */
	void UseArenaCamera(ACombatArena* Arena);

	/**
	 * @brief Передаёт сессии команду подготовки следующего этапа после записи прогресса.
	 * @param[in] SaveSlot Выделенный слот PIE. None использует открытый слот.
	 * @return true, если сессия приняла команду продолжения.
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Combat|Stage")
	bool RequestNextStage(FName SaveSlot);

	/**
	 * @brief Передаёт сессии подтверждение выхода из пробного боя.
	 * @param[in] SaveSlot Выделенный слот текущего PIE. None использует открытый слот.
	 * @return true, если сессия приняла выход или повтор записи после ошибки.
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Combat|Session")
	bool RequestExitCombat(FName SaveSlot);

	/** @brief Передаёт сессии открытие или закрытие окна подтверждения. */
	bool SetLeaveConfirmationOpen(bool bOpen);

	/** @brief Сессия вызывает метод после записи и освобождения боевых ресурсов. */
	void NotifyCombatExitReady();

	/** @brief Даёт Blueprint точку подключения перехода карты после подтверждения сохранения. */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Session")
	FOnCombatExitReady OnCombatExitReady;

protected:
	/**
	 * @brief Показывает курсор и включает GameAndUI при начале игры.
	 *
	 * Unreal вызывает метод для контроллера. Курсор остаётся видимым при захвате, а UI первым получает возможность обработать ввод.
	 */
	void BeginPlay() override;

	/**
	 * @brief Подключает IA_Combat_Click и IMC_Combat при настройке ввода контроллера.
	 *
	 * Unreal вызывает метод при инициализации ввода игрока. Если подсистема, компонент или ассет отсутствуют, контроллер пишет предупреждение.
	 * При успехе в логе появляется Combat input context added.
	 */
	void SetupInputComponent() override;

	/**
	 * @brief Снимает боевой контекст при завершении контроллера.
	 * @param[in] EndPlayReason Причина завершения, передаваемая базовому контроллеру.
	 * @post Контекст снят, если подсистема, в которую его добавили, ещё существует.
	 */
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** @brief Получает сессию из GameMode текущего мира для передачи боевой команды. */
	UCombatSession* GetActiveCombatSession() const;

	/** @brief Создаёт корневой HUD в области локального игрока и связывает его с сессией. */
	void AddCombatWidget();

	/** @brief Отсоединяет виджет от сессии и удаляет его с экрана. */
	void RemoveCombatWidget();

	/** @brief Класс WBP_Root_Combat из Class Defaults боевого контроллера. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|UI")
	TSubclassOf<UCombatRootWidget> CombatRootWidgetClass;

	/** @brief Экземпляр HUD на время входа в L_Combat. */
	UPROPERTY(Transient)
	TObjectPtr<UCombatRootWidget> CombatWidget;

	/**
	 * @brief Преобразует начало IA_Combat_Click в запрос к текущей сессии.
	 *
	 * Enhanced Input вызывает метод на Started. Если сессии нет, обработка заканчивается.
	 * Для Ignore контроллер пишет результат в лог. Hit и Miss отправляет сессии с проверенным экземпляром цели.
	 */
	void HandleCombatClick();

	/**
	 * @brief Трассирует луч от курсора по областям выбранного противника.
	 * @param[in] Target Экземпляр из сессии, по которому проверяется клик.
	 * @return Hit для области попадания, Miss для области промаха, иначе Ignore. Без цели или луча возвращает Ignore.
	 */
	ECombatClickResult ClassifyCombatClick(const ACombatEnemy* Target) const;

	/**
	 * @brief Контекст IMC_Combat из Class Defaults BP_Combat_PlayerController.
	 *
	 * Подключается к локальному игроку в SetupInputComponent() и снимается в EndPlay().
	 * Пустая ссылка блокирует подключение боевого ввода.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
	TObjectPtr<UInputMappingContext> CombatMappingContext;

	/**
	 * @brief Действие кликовой атаки из Class Defaults BP_Combat_PlayerController.
	 *
	 * В SetupInputComponent() действие привязывается к обработчику. Пустая ссылка оставляет боевой ввод неподключённым.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
	TObjectPtr<UInputAction> CombatClickAction;

	/**
	 * @brief Слабая ссылка на подсистему, которой контроллер назначил IMC_Combat.
	 *
	 * Появляется после AddMappingContext() и очищается при снятии контекста в EndPlay().
	 * Если подсистема уже уничтожена, снимать контекст с неё не требуется.
	 */
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> CombatInputSubsystem;
};

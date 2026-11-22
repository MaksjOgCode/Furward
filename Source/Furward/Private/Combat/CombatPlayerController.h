// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает боевой и общий ввод, камеру и команды, которые контроллер передаёт сессии.
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
class UUserWidget;
enum class ECombatClickResult : uint8;

/** @brief Сигнал о завершении боя после записи актуальной ревизии профиля. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatExitReady);



/**
 * @brief Обрабатывает ввод локального игрока на L_Combat.
 *
 * Unreal создаёт BP_Combat_PlayerController по настройкам GameMode. Локальный контроллер создаёт корневой HUD.
 * Контроллер существует в игровом мире до выхода с карты и подключает боевой и общий контексты локального игрока.
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
	 * @param[in] SaveSlot Выделенный слот. None использует открытый слот.
	 * @return true, если сессия приняла команду продолжения.
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Combat|Stage")
	bool RequestNextStage(FName SaveSlot);

	/** @brief Передаёт сессии выбор фарма из окна поражения. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Session")
	bool RequestEnterFarm();

	/** @brief Передаёт сессии команду возврата из активного фарма в экспедицию. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Session")
	bool RequestReturnToExpedition();

	/**
	 * @brief Передаёт сессии подтверждение выхода из пробного боя.
	 * @param[in] SaveSlot Имя слота. None использует открытый профилем слот.
	 * @return true, если сессия приняла выход или повтор записи после ошибки.
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Combat|Session")
	bool RequestExitCombat(FName SaveSlot);

	/**
	 * @brief Передаёт команду слота от клавиши или Blueprint.
	 * @param[in] SlotIndex Индекс слота с нуля.
	 * @return true, если сессия приняла команду и способность сработала.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Input")
	bool RequestUseAbilitySlot(int32 SlotIndex);

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
	 * @brief Подключает IMC_Common для паузы и IMC_Combat для кликовой атаки и слотов.
	 *
	 * Unreal вызывает метод при инициализации ввода игрока. Если подсистема, компонент или ассет отсутствуют, контроллер пишет предупреждение.
	 * Успешное подключение каждого контекста подтверждается отдельной записью в логе.
	 */
	void SetupInputComponent() override;

	/**
	 * @brief Удаляет виджеты и снимает подключённые контексты при завершении контроллера.
	 * @param[in] EndPlayReason Причина завершения, передаваемая базовому контроллеру.
	 * @post Контексты сняты, если подсистема, в которую их добавили, ещё существует.
	 */
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** @brief Получает сессию из GameMode текущего мира для передачи боевой команды. */
	UCombatSession* GetActiveCombatSession() const;

	/** @brief Создаёт корневой HUD в области локального игрока и связывает его с сессией. */
	void AddCombatWidget();

	/** @brief Отсоединяет виджет от сессии и удаляет его с экрана. */
	void RemoveCombatWidget();

	/** @brief Обрабатывает начало IA_Pause и переключает окно с разрешения сессии. */
	void HandlePauseToggle();

	/** @brief Убирает экранный экземпляр паузы и освобождает ссылку контроллера. */
	void RemovePauseWidget();

	/** @brief Класс WBP_Root_Combat из Class Defaults боевого контроллера. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|UI")
	TSubclassOf<UCombatRootWidget> CombatRootWidgetClass;

	/** @brief Экземпляр HUD на время входа в L_Combat. */
	UPROPERTY(Transient)
	TObjectPtr<UCombatRootWidget> CombatWidget;

	/** @brief Общий виджет паузы проекта из Class Defaults. */
	UPROPERTY(EditDefaultsOnly, Category = "Common|UI")
	TSubclassOf<UUserWidget> PauseWidgetClass;

	/** @brief Экземпляр окна, который существует, пока открыта пауза. */
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> PauseWidget;

	/**
	 * @brief Преобразует начало IA_Combat_Click в запрос к текущей сессии.
	 *
	 * Enhanced Input вызывает метод на Started. Если сессии нет, обработка заканчивается.
	 * Для Ignore контроллер пишет результат в лог. Hit и Miss отправляет сессии с проверенным экземпляром цели.
	 */
	void HandleCombatClick();

	/** @brief Передаёт в общий обработчик индекс, привязанный к действию Enhanced Input. */
	void HandleAbilitySlot(int32 SlotIndex);

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

	/** @brief Действия слотов в том же порядке, что и слоты героя. Пустой элемент сохраняет индекс. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
	TArray<TObjectPtr<UInputAction>> CombatAbilityActions;

	/** @brief Общий контекст локального игрока для команд, доступных на разных картах. */
	UPROPERTY(EditDefaultsOnly, Category = "Common|Input")
	TObjectPtr<UInputMappingContext> CommonMappingContext;

	/** @brief Действие открытия и закрытия паузы из IMC_Common. */
	UPROPERTY(EditDefaultsOnly, Category = "Common|Input")
	TObjectPtr<UInputAction> PauseAction;

	/**
	 * @brief Слабая ссылка на подсистему, которой контроллер назначил контексты.
	 *
	 * Выбирается в SetupInputComponent() и очищается при снятии контекстов в EndPlay().
	 * Если подсистема уже уничтожена, снимать контексты с неё не требуется.
	 */
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> LocalInputSubsystem;
};

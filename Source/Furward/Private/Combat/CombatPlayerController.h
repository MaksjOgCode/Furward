// Copyright © 2026 Maksj.OG

/**
 * @file
 * @brief Описывает ввод, боевую камеру и передачу запроса кликовой атаки в сессию.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CombatPlayerController.generated.h"



class ACombatArena;
class ACombatEnemy;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
enum class ECombatClickResult : uint8;



/**
 * @brief Обрабатывает ввод локального игрока на L_Combat.
 *
 * Unreal создаёт BP_Combat_PlayerController по настройкам GameMode.
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

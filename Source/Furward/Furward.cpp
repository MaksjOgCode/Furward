// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * @file
 * @brief Объявляет Furward основным игровым модулем, который Unreal загружает при запуске проекта.
 */

#include "Furward.h"
#include "Modules/ModuleManager.h"

// Связываем имя модуля из Furward.uproject со стандартной реализацией Unreal.
IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, Furward, "Furward" );

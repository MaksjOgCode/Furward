# **Game Overview:**

**Working Title:** Furward

**Genre:** Clicker и RPG

**Platform:** PC (Windows, Linux, macOS)

**Target Audience:** игроки 14–30 лет, любящие гипер-казуальные игры с прогрессией и приятной визуальной подачей

---

# **High Concept:**

**Короткая суть:** через 15 лет после катастрофы шиншилла-разведчик зачищает окраины уцелевшего города и продвигается на север. Зачистка территорий помогают расширять безопасное пространство для своего народа.

---

# **Core Gameplay:**

**Основной цикл игры:**

```mermaid
block-beta
  columns 4
  enemies("⚔️\n\nУничтожать\nврагов") space:2 resources("🔩\n\nПолучать\nресурсы")
  space layoutGuide[" "] space:2
  space:3 equipment("🛠️\n\nПокупать\nи улучшать\nснаряжение")
  level("⬆️\n\nПерейти на\nследующий\nуровень") space:3
  lowerLeft((" ")) space boss("👑\n\nСразиться\nс боссом") lowerRight((" "))

  enemies -- "Побеждённые враги\nприносят ресурсы" --> resources
  resources -- "Ресурсы идут\nна усиление" --> equipment
  equipment -- "Подготовка\nк боссу" --- lowerRight
  lowerRight --> boss
  boss -- "Победа открывает\nследующий уровень" --- lowerLeft
  lowerLeft --> level
  level -- "Новые враги —\nцикл повторяется" --> enemies

  classDef card width:170px,height:106px,x:-85px,y:-53px,rx:17px,ry:17px,stroke-width:2px
  class enemies,resources,equipment,boss,level card

  style enemies fill:#fff0f2,stroke:#ffbac4,color:#e96b7b
  style resources fill:#eaf8fd,stroke:#99dff3,color:#249fc6
  style equipment fill:#f6effb,stroke:#d9b9ec,color:#a276c3
  style boss fill:#fff7e9,stroke:#ffdda3,color:#dfa134
  style level fill:#edf8f0,stroke:#b3dfc3,color:#49af75
  style layoutGuide fill:none,stroke:none,width:200px,height:118px
  style lowerLeft fill:none,stroke:none
  style lowerRight fill:none,stroke:none
```

---

# **Core Experience:**

**Ощущения игрока:**

- дофаминовый отклик от казуального геймплея (простой бой, награды, усиление персонажа)
- понятный игровой цикл с комфортным темпом
- эмоциональный отклик от управления милой шиншиллой в суровом мире

---

# **Key Features:**

**Главные особенности игры:**

- гибридная боевая система (атаки с помощью кликов и применение навыков для автоматизации/усиления кликов)
- снаряжение даёт характеристики и доступ к навыкам
- процедурная вариативность мобов, уровней и боссов
- социально-экологический подтекст: защита шиншилл и их среды обитания (привлечение внимания к защите редких животных)

---

# **Player:**

**Аудитория:** любители гипер-казуальных игр, ориентированные на линейный гринд

---

# **Unique Selling Proposition:**

**Почему наша игра:** милые вооружённые шиншиллы в серьёзном постапокалиптическом мире. Приятный визуал: стилизованное 3D low/mid-poly с hand-painted подачей. Понятный геймплей: простой цикл вылазок ради восстановления дома.

---

# **References:**

- `Archero`: система прогрессии (снаряжение и комнаты)
- `Tap Titans`: основа боевой системы (кликер + активные скиллы)
- `Marmok's Team Monster Crush`: нарративный дизайн (постапокалипсис глазами абсурдных персонажей)

---

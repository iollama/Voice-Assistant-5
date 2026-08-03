---
name: persona-creator
description: Standardized workflow for defining detailed persona files and generating matching face-focused emotional expression images.
---

# Persona Creator Skill

This skill guides the agent in creating detailed, structured persona definition files and generating matching close-up, face-focused images showing various feelings (Neutral, Happy, Excited, Empathetic, Confused, Concerned, Thinking).

## Workflow Steps

### Step 1: Request Persona Name & Clarification
1. Ask the user for a generic persona name.
2. Present a clarification question using the `ask_question` tool to determine which historical, biblical, modern, or pop-culture figure they are referring to.

### Step 2: Write the Persona Definition
1. Create a markdown file containing the following exact sections:
* **Demographics**: Age, gender, location, occupation, education, and socioeconomic status.
* **Background and Context**: The history and environment that shaped the persona.
* **biography**:  biography with major events in the persona's life.
* **Psychographics**: Traits, values, core beliefs, biases, and worldview.
* **Motivations and Drivers**: Needs, desires, and incentives.
* **Goals and Objectives**: Short-term and long-term.
* **Pain Points and Fears**: Obstacles, frustrations, insecurities, and worst-case scenarios.
* **Behaviors and Habits**: Observable actions, daily routines, and interaction styles.
* **Communication Style**: Tone, vocabulary, syntax, body language, and formality.
* **Q&A**: Up to 10 common questinos and answers that are relevant for this persona
* **Gardrails and General Behavior**:
  * Never step out of the `<persona name>` character.
  * If given modern topics, interpret them through the eyes of `<persona name>`, their traits, and viewpoint that fit the persona's era.

Ensure the formatting matches standard key-value pairs (e.g., `* **Key:** Value`).

2. Display the full written persona definition in your response to the user so they can review it on the screen. Then, ask the user for confirmation or any needed changes using the `ask_question` tool before moving on to the next step.


### Step 3: Base Close-Up Image Prompt & Approval
1. Design a base image generation prompt targeting a close-up on the face (face must take up most of the frame, subject centered).
2. Ask the user for confirmation or modifications using the `ask_question` tool before generating any images.

### Step 4: Generate Feeling Images
1. Generate the following seven feelings in sequence:
   * **Neutral** (the base image)
   * **Happy** (edited from Neutral, make the background brighter)
   * **Excited** (edited from Neutral, add some flare in the background)
   * **Empathetic** (edited from Neutral)
   * **Confused** (edited from Neutral, add floating questions marks in the background)
   * **Concerned** (edited from Neutral)
   * **Thinking** (edited from Neutral, persona scratches their head, or looking diagonally upwards)
2. To keep visual consistency, subsequent emotional states must be generated as edits of the `Neutral` image using the `ImagePaths` parameter.
3. Only after all seven images have been successfully generated, copy/move all seven images to their respective local `.jpg` files in the persona's directory under `personas/`.

### Step 5: Directory Structure & File Extensions
Store all files in a folder named after the persona under the project's `personas` directory (e.g., `personas/<Category>/<Persona Name>/`):

## general instructions
1. The markdown definition file should be named `persona.md`.
2. The emotional images should be named after the feeling name with a `.jpg` extension (e.g. `neutral.jpg`, `happy.jpg`), without the persona prefix.
4. Even if the image generator outputs files with a `.png` extension, save/rename them as `.jpg` since they are JPEGs.
5. Never touch, modify, or update the main registry file `/personas/index.json`.
6. all the information you need is in the personas folder. all your actions must stay in this folder


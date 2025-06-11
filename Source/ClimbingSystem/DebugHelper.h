#pragma once

namespace Debug
{
	static void Print(const FString& Message, const FColor& Color = FColor::MakeRandomColor(), const int32 InKey = -1)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(InKey, 5.f, Color, Message);
			UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
		}
	}

	static void Print(const FString& Message, const float FloatValue, const FColor& Color = FColor::MakeRandomColor(), const int32 InKey = -1)
	{
		if (GEngine)
		{
			const FString FinalMsg = Message + TEXT(": ") + FString::SanitizeFloat(FloatValue);
			GEngine->AddOnScreenDebugMessage(InKey, 5.f, Color, FinalMsg);
			UE_LOG(LogTemp, Warning, TEXT("%s"), *FinalMsg);
		}
	}
}

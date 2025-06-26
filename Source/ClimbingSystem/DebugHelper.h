#pragma once

namespace DebugH
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

	static void Print(const FString& Message, const bool Bool2Print, const FColor& Color = FColor::MakeRandomColor(), const int32 InKey = -1)
	{
		const FString MessageP = Message + FString::Printf(TEXT(" bool value is: %s"), Bool2Print ? TEXT("True") : TEXT("False"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(InKey, 5.f, Color, MessageP);
			UE_LOG(LogTemp, Warning, TEXT("%s"), *MessageP);
		}
	}
}

#pragma once

// --- Reflection-like no-op macros (future header tool will expand these) ---
#ifndef ACE_CLASS
#define ACE_CLASS(...)
#endif

#ifndef ACE_STRUCT
#define ACE_STRUCT(...)
#endif

#ifndef ACE_ENUM
#define ACE_ENUM(...)
#endif

#ifndef ACE_INTERFACE
#define ACE_INTERFACE(...)
#endif

#ifndef ACE_PROPERTY
#define ACE_PROPERTY(...)
#endif

#ifndef ACE_FUNCTION
#define ACE_FUNCTION(...)
#endif

#ifndef GENERATED_BODY
#define GENERATED_BODY(...)
#endif

// Optional, for places you want explicit "this class participates in reflection"
#ifndef ACE_REFLECT
#define ACE_REFLECT(...)
#endif

// --- UE-style front-door aliases (map to ACE_* so you can author UE-like code) ---
#ifndef UCLASS
#define UCLASS(...) ACE_CLASS(__VA_ARGS__)
#endif
#ifndef USTRUCT
#define USTRUCT(...) ACE_STRUCT(__VA_ARGS__)
#endif
#ifndef UENUM
#define UENUM(...)  ACE_ENUM(__VA_ARGS__)
#endif
#ifndef UINTERFACE
#define UINTERFACE(...) ACE_INTERFACE(__VA_ARGS__)
#endif
#ifndef UPROPERTY
#define UPROPERTY(...) ACE_PROPERTY(__VA_ARGS__)
#endif
#ifndef UFUNCTION
#define UFUNCTION(...) ACE_FUNCTION(__VA_ARGS__)
#endif
// GENERATED_BODY is already defined above; keep as-is.

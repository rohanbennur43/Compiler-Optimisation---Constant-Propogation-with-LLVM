; ModuleID = 'test1.c'
source_filename = "test1.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @test() #0 {
entry:
  %y = alloca i32, align 4
  %x = alloca i32, align 4
  %z = alloca i32, align 4
  store i32 3, i32* %y, align 4
  store i32 10, i32* %x, align 4
  store i32 11, i32* %y, align 4
  %0 = load i32, i32* %x, align 4
  %1 = load i32, i32* %y, align 4
  %cmp = icmp sgt i32 %0, %1
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %2 = load i32, i32* %x, align 4
  %add = add nsw i32 %2, 1
  store i32 %add, i32* %x, align 4
  %3 = load i32, i32* %x, align 4
  %add1 = add nsw i32 %3, 2
  store i32 %add1, i32* %y, align 4
  br label %if.end

if.else:                                          ; preds = %entry
  %4 = load i32, i32* %x, align 4
  %5 = load i32, i32* %y, align 4
  %add2 = add nsw i32 %4, %5
  store i32 %add2, i32* %z, align 4
  store i32 4, i32* %x, align 4
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  ret void
}

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Ubuntu clang version 14.0.0-1ubuntu1.1"}

; ModuleID = '../../../test/phase2/test_compare.c'
source_filename = "../../../test/phase2/test_compare.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local void @test() #0 {
entry:
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  %z = alloca i32, align 4
  %v = alloca i32, align 4
  store i32 1, i32* %a, align 4
  store i32 2, i32* %b, align 4
  %0 = load i32, i32* %a, align 4
  %1 = load i32, i32* %b, align 4
  %cmp = icmp ne i32 %0, %1
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %2 = load i32, i32* %b, align 4
  %3 = load i32, i32* %a, align 4
  %add = add nsw i32 %2, %3
  store i32 %add, i32* %z, align 4
  br label %if.end

if.else:                                          ; preds = %entry
  %4 = load i32, i32* %b, align 4
  %5 = load i32, i32* %a, align 4
  %sub = sub nsw i32 %4, %5
  store i32 %sub, i32* %z, align 4
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  br label %do.body

do.body:                                          ; preds = %do.cond, %if.end
  %6 = load i32, i32* %z, align 4
  %cmp1 = icmp slt i32 %6, 0
  br i1 %cmp1, label %if.then2, label %if.else4

if.then2:                                         ; preds = %do.body
  %7 = load i32, i32* %z, align 4
  %add3 = add nsw i32 %7, 1
  store i32 %add3, i32* %v, align 4
  br label %if.end6

if.else4:                                         ; preds = %do.body
  %8 = load i32, i32* %z, align 4
  %sub5 = sub nsw i32 %8, 1
  store i32 %sub5, i32* %v, align 4
  br label %if.end6

if.end6:                                          ; preds = %if.else4, %if.then2
  %9 = load i32, i32* %b, align 4
  %sub7 = sub nsw i32 %9, 1
  store i32 %sub7, i32* %b, align 4
  br label %do.cond

do.cond:                                          ; preds = %if.end6
  %10 = load i32, i32* %b, align 4
  %cmp8 = icmp sge i32 %10, 0
  br i1 %cmp8, label %do.body, label %do.end, !llvm.loop !6

do.end:                                           ; preds = %do.cond
  ret void
}

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Ubuntu clang version 14.0.0-1ubuntu1.1"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}

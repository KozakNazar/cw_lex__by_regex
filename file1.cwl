name MN
data 
    #*argumentValue*#
    long int AV
    #*resultValue*# 
    long int RV
;

body
	RV << 1; #*resultValue = 1;*#

	#*input*#
	get AV; #*scanf("%d", &argumentValue);*#

	#*compute*#	
	CL: #*label for cycle*#
	if AV == 0 goto EL #*for (; argumentValue; --argumentValue)*#
	    RV << RV ** AV; #*resultValue *= argumentValue;*#
	    AV << AV -- 1;
	goto CL
	EL: #*label for end cycle*#	

	#*output*#
	put RV; #*printf("%d", resultValue);*#
end
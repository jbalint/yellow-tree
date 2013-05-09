public class TestTypes {
    Object oarray[];
    boolean zarray[];
    byte barray[];
    char carray[];
    short sarray[];
    int iarray[];
    long jarray[];
    float farray[];
    double darray[];

    public TestTypes() {
	oarray = new Object[5];
	zarray = new boolean[5];
	barray = new byte[5];
	carray = new char[] {' ', ' ', ' ', ' ', ' '};
	sarray = new short[5];
	iarray = new int[5];
	jarray = new long[5];
	farray = new float[5];
	darray = new double[5];
    }

    public void assign() {
	oarray[2] = "number2";
	zarray[3] = true;
	barray[4] = -100;
	carray[0] = 'A';
	sarray[1] = 85;
	iarray[2] = 850;
	jarray[3] = 8500;
	farray[1] = (float)50.1;
	darray[2] = 40.12345;
    }

    public void setArray(Object o[]) {
	oarray = o;
    }
    public void setArray(boolean z[]) {
	zarray = z;
    }
    public void setArray(byte b[]) {
	barray = b;
    }
    public void setArray(char c[]) {
	carray = c;
    }
    public void setArray(short s[]) {
	sarray = s;
    }
    public void setArray(int i[]) {
	iarray = i;
    }
    public void setArray(long j[]) {
	jarray = j;
    }
    public void setArray(float f[]) {
	farray = f;
    }
    public void setArray(double d[]) {
	darray = d;
    }
}


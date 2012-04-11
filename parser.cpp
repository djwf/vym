#include "parser.h"

#include <QDebug>
#include <QRegExp>
#include <iostream>

Command::Command (const QString &n, SelectionType st)
{
    name=n;
    selectionType=st;
}

QString Command::getName()
{
    return name;
}

void Command::addPar (ParameterType t, bool opt, const QString &c)  
{
    parTypes.append (t);
    parOpts.append (opt);
    parComments.append (c);
}

int Command::parCount()
{
    return parTypes.count();
}

Command::ParameterType Command::getParType (int n)
{
    if (n>=0 and n<parTypes.count() )
    {
	return parTypes.at(n);
    }
    qDebug()<<"Command::getParType n out of range";
    return Undefined;
}

bool Command::isParOptional (int n)
{
    if (n>=0 and n<parTypes.count() )
    {
	return parOpts.at(n);
    }
    qDebug()<<"Command::isParOpt n out of range";
    return false;
}

QString  Command::getParComment(int n)
{
    if (n>=0 and n<parTypes.count() )
    {
	return parComments.at(n);
    }
    qDebug()<<"Command::getParComment n out of range";
    return QString();
}

Parser::Parser()
{
    initParser();
}

void Parser::initParser()
{
    initAtom();
    current=-1;
}

void Parser::initAtom()
{
    atom="";
    com="";
    paramList.clear();
    resetError();
}

void Parser::parseAtom (QString s)
{
    initAtom();
    atom=s;
    QRegExp re;
    int pos;

    // Strip WS at beginning
    re.setPattern ("\\w");
    re.setMinimal (true);
    pos=re.indexIn (atom);
    if (pos>=0)
	s=s.right(s.length()-pos);

    // Get command
    re.setPattern ("\\b(.*)(\\s|\\()");
    pos=re.indexIn (s);
    if (pos>=0)
	com=re.cap(1);

    // Get parameters
    paramList.clear();
    re.setPattern ("\\((.*)\\)");
    pos=re.indexIn (s);
    //qDebug() << "  s="<<s;
    //qDebug() << "com="<<com<<"  pos="<<pos;
    if (pos>=0)
    {
	QString s=re.cap(1);
	QString a;
	bool inquote=false;
	pos=0;
	if (!s.isEmpty())
	{
	    while (pos<s.length())
	    {
		if (s.at(pos)=='\"') 
		{
		    if (inquote)
			inquote=false;
		    else    
			inquote=true;
		}

		if (s.at(pos)==',' && !inquote)
		{
		    a=s.left(pos);
		    paramList.append(a);
		    s=s.right(s.length()-pos-1);
		    pos=0;
		} else
		    pos++;
	    }
	    paramList.append (s);
	}   
    }	
}

QString Parser::getAtom()
{
    return atom;
}

QString Parser::getCommand()
{
    return com;
}

QStringList Parser::getParameters()
{
    return paramList;
}

int Parser::parCount()
{
    return paramList.count();
}

QString Parser::errorMessage()
{
    QString l;
    switch (errLevel)
    {
	case NoError: l="No Error";
	case Warning: l="Warning";
	case Aborted: l="Aborted";
    }
    return QString ("Error Level: '%1'  Command: '%2' Description: '%3'")
	.arg(l).arg(com).arg(errDescription);
}

QString Parser::errorDescription()
{
    return errDescription;
}

ErrorLevel Parser::errorLevel()
{
    return errLevel;
}

void Parser::setError(ErrorLevel level, const QString &description)
{
    errDescription=description;
    errLevel=level;
}

bool Parser::checkParameters()
{
    foreach (Command *c, commands)
    {
	if (c->getName() == com)
	{
	    qDebug()<<"  Found: "<<com;

	    // Check for number of parameters
	    int optPars=0;
	    for (int i=0; i < c->parCount(); i++ )
		if (c->isParOptional(i) ) optPars++;
	    if (paramList.count() < (c->parCount() - optPars) ||
	        paramList.count() > c->parCount() )
	    {
		QString expected;
		if (optPars>0)
		    expected=QString("%1..%2").arg(c->parCount()-optPars).arg(c->parCount() );
		else 
		    expected=QString().setNum(c->parCount());
		errDescription=QString("Wrong number of parameters: Expected %1, but found %2").arg(expected).arg(paramList.count());
		errLevel=Aborted;
		return false;
	    }

	    // Check for types of parameters	//FIXME-2 check related functions...
	    bool ok;
	    for (int i=0; i < paramList.count(); i++ )
	    {	
		switch (c->getParType(i) )
		{
		    case Command::String:
			parString (ok,i);
			break;
		    case Command::Int:	
			parInt (ok,i);
			break;
		    case Command::Double:	
			parDouble (ok,i);
			break;
		    case Command::Color:	
			parColor (ok,i);
			break;
		    case Command::Bool:	
			parBool (ok,i);
		    default: ok=false;	
		}
		if (!ok)
		{
		    errLevel=Aborted;
		    errDescription=QString("Parameter %1 has wrong type").arg(i);
		    return false;
		}
	    }
	    return true;
	}    
    } 
    setError (Aborted,"Unknown command");
    return false;
}

void Parser::resetError ()
{
    errMessage="";
    errDescription="";
    errLevel=NoError;
}

bool Parser::checkParCount (QList <int> plist)
{
    QStringList expList;
    QString expected;
    for (int i=0; i<plist.count();i++)
    {
	if (checkParCount (plist[i])) 
	{
	    resetError();
	    return true;
	}
	expList.append(QString().setNum(plist[i]));
    }	
    expected=expList.join(",");	
    errDescription=QString("Wrong number of parameters: Expected %1, but found %2").arg(expected).arg(paramList.count());
    return false;
}

bool Parser::checkParCount (const int &expected)
{
    if (paramList.count()!=expected)
    {
	errLevel=Aborted;
	errDescription=QString("Wrong number of parameters: Expected %1, but found %2").arg(expected).arg(paramList.count());
	return false;
    } 
    return true;    
}

bool Parser::checkParIsInt(const int &index)
{
    bool ok;
    if (index > paramList.count())
    {
	errLevel=Aborted;
	errDescription=QString("Parameter index %1 is outside of parameter list").arg(index);
	return false;
    } else
    {
	paramList[index].toInt (&ok, 10);
	if (!ok)
	{
	    errLevel=Aborted;
	    errDescription=QString("Parameter %1 is not an integer").arg(index);
	    return false;
	} 
    }	
    return true;
}

bool Parser::checkParIsDouble(const int &index)
{
    bool ok;
    if (index > paramList.count())
    {
	errLevel=Aborted;
	errDescription=QString("Parameter index %1 is outside of parameter list").arg(index);
	return false;
    } else
    {
	paramList[index].toDouble (&ok);
	if (!ok)
	{
	    errLevel=Aborted;
	    errDescription=QString("Parameter %1 is not double").arg(index);
	    return false;
	} 
    }	
    return true;
}

int Parser::parInt (bool &ok,const uint &index)
{
    if (checkParIsInt (index))
	return paramList[index].toInt (&ok, 10);
    ok=false;
    return 0;
}

QString Parser::parString (bool &ok,const int &index)
{
    // return the string at index, this could be also stored in
    // a variable later
    QString r;
    QRegExp re("\"(.*)\"");
    int pos=re.indexIn (paramList[index]);
    if (pos>=0)
    {
	r=re.cap (1);
	ok=true;
    } else    
    {
	r="";
	ok=false;
    }
    return r;
}

bool Parser::parBool (bool &ok,const int &index)
{
    // return the bool at index, this could be also stored in
    // a variable later
    QString r;
    ok=true;
    QString p=paramList[index];
    if (p=="true" || p=="1")
	return true;
    else if (p=="false" || p=="0")
	return false;
    ok=false;
    return ok;
}

QColor Parser::parColor(bool &ok,const int &index)
{
    // return the QColor at index
    ok=false;
    QString r;
    QColor c;
    QRegExp re("\"(.*)\"");
    int pos=re.indexIn (paramList[index]);
    if (pos>=0)
    {
	r=re.cap (1);
	c.setNamedColor(r);
	ok=c.isValid();
    }	
    return c;
}

double Parser::parDouble (bool &ok,const int &index)
{
    if (checkParIsDouble (index))
	return paramList[index].toDouble (&ok);
    ok=false;
    return 0;
}

void Parser::setScript(const QString &s)
{
    script=s;
}   

QString Parser::getScript()
{
    return script;
}   

void Parser::execute()
{
    current=0;
}   

bool Parser::next()
{
    int start=current;
    if (current<0) execute();
    if (current+1>=script.length()) return false;

    bool inBracket=false;
    while (true)
    {
	//qDebug() <<"current="<<current<< "   start="<<start<<"  length="<<script.length();

	// Check if we are inside a string
	if (script.at(current)=='"')
	{
	    if (inBracket)
		inBracket=false;
	    else    
		inBracket=true;
	}

	// Check if we are in a comment
	if (!inBracket && script.at(current)=='#')
	{
	    while (script.at(current)!='\n')
	    {
		current++;
		if (current+1>=script.length()) 
		    return false;
	    }
	    start=current;
	}

	// Check for end of atom
	if (!inBracket && script.at(current)==';')
	{
	    atom=script.mid(start,current-start);
	    current++;
	    return true;
	}
	
	// Check for end of script
	if (current+1>=script.length() )
	{
	    if (inBracket)
	    {
		setError (Aborted,"Runaway string");
		return false;
	    } else
	    {
		atom=script.mid(start);
		return true;
	    }
	}
	current++;
    }
}   

QStringList Parser::getCommands() 
{
    QStringList list;
    foreach (Command *c, commands)
	list.append (c->getName() );
    return list;	
}

void Parser::addCommand (Command *c)
{
    commands.append (c);
}

